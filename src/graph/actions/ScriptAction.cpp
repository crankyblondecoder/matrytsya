#include "ScriptAction.hpp"

#include <cstdlib>

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphException.hpp"
#include "../GraphNode.hpp"
#include "../../lua/lua.hpp"

namespace
{
	// Globals the base library opens with that reach outside the sandbox (filesystem or stdio); stripped once
	// the state is created.
	const char* const DISALLOWED_GLOBALS[] = {"dofile", "loadfile", "print", "warn"};
}

ScriptAction::~ScriptAction()
{
	if(_luaState) lua_close(_luaState);
}

ScriptAction::ScriptAction(GraphNodeHandle& initNode)
	: GraphAction(initNode, 32)
{
	_addFlag(SCRIPT_GRAPH_ACTION);

	_luaState = lua_newstate(__alloc, this, luaL_makeseed(0));

	if(!_luaState) throw GraphException(GraphException::SCRIPT_STATE_BAD_ALLOC);

	// Only libraries with no filesystem, process, environment or introspection access are opened. io, os,
	// package and debug are deliberately left closed.
	luaL_openselectedlibs(_luaState,
		LUA_GLIBK | LUA_COLIBK | LUA_MATHLIBK | LUA_STRLIBK | LUA_TABLIBK | LUA_UTF8LIBK, 0);

	for(const char* global : DISALLOWED_GLOBALS)
	{
		lua_pushnil(_luaState);
		lua_setglobal(_luaState, global);
	}

	lua_pushcfunction(_luaState, __safeLoad);
	lua_setglobal(_luaState, "load");

	// Keep this table around as the shared base every node's isolated environment reads through to; it is
	// never installed as the live global table again after this point.
	lua_pushglobaltable(_luaState);
	_sharedEnvRef = luaL_ref(_luaState, LUA_REGISTRYINDEX);
}

void ScriptAction::_apply(GraphNode* target)
{
	ScriptActionTarget* scriptTarget = target -> getScriptActionTarget();

	if(scriptTarget)
	{
		__installIsolatedEnv();

		scriptTarget -> invoke(_luaState);

		__captureLastNodeEnv();
	}
}

void ScriptAction::_complete()
{
}

void ScriptAction::_shareGlobal(const char* name, bool value)
{
	lua_pushboolean(_luaState, value);
	__shareGlobal(name);
}

void ScriptAction::_shareGlobal(const char* name, int value)
{
	lua_pushinteger(_luaState, value);
	__shareGlobal(name);
}

void ScriptAction::_shareGlobal(const char* name, double value)
{
	lua_pushnumber(_luaState, value);
	__shareGlobal(name);
}

void ScriptAction::_shareGlobal(const char* name, const char* value)
{
	lua_pushstring(_luaState, value);
	__shareGlobal(name);
}

void ScriptAction::__shareGlobal(const char* name)
{
	// Stack on entry: [..., value]
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, _sharedEnvRef); // [..., value, sharedTable]
	lua_insert(_luaState, -2); // [..., sharedTable, value]
	lua_setfield(_luaState, -2, name); // sharedTable[name] = value; [..., sharedTable]
	lua_pop(_luaState, 1); // [...]
}

bool ScriptAction::_getGlobal(const char* name, bool& value)
{
	if(!__getGlobal(name) || !lua_isboolean(_luaState, -1))
	{
		lua_pop(_luaState, 1);
		return false;
	}

	value = lua_toboolean(_luaState, -1);
	lua_pop(_luaState, 1);
	return true;
}

bool ScriptAction::_getGlobal(const char* name, int& value)
{
	if(!__getGlobal(name) || !lua_isinteger(_luaState, -1))
	{
		lua_pop(_luaState, 1);
		return false;
	}

	value = static_cast<int>(lua_tointeger(_luaState, -1));
	lua_pop(_luaState, 1);
	return true;
}

bool ScriptAction::_getGlobal(const char* name, double& value)
{
	if(!__getGlobal(name) || !lua_isnumber(_luaState, -1))
	{
		lua_pop(_luaState, 1);
		return false;
	}

	value = lua_tonumber(_luaState, -1);
	lua_pop(_luaState, 1);
	return true;
}

bool ScriptAction::_getGlobal(const char* name, const char*& value)
{
	if(!__getGlobal(name) || !lua_isstring(_luaState, -1))
	{
		lua_pop(_luaState, 1);
		return false;
	}

	value = lua_tostring(_luaState, -1);
	lua_pop(_luaState, 1);
	return true;
}

bool ScriptAction::__getGlobal(const char* name)
{
	// Reading via the last node's env (when one exists) falls through its metatable to the shared table
	// automatically, so this also covers the _shareGlobal()-only fallback case.
	int envRef = _lastNodeEnvRef ? _lastNodeEnvRef : _sharedEnvRef;

	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_luaState, -1, name); // [env, value]
	lua_remove(_luaState, -2); // [value]
	return !lua_isnil(_luaState, -1);
}

void ScriptAction::__installIsolatedEnv()
{
	lua_newtable(_luaState); // env
	lua_newtable(_luaState); // metatable
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, _sharedEnvRef); // shared table
	lua_setfield(_luaState, -2, "__index"); // metatable.__index = shared table
	lua_setmetatable(_luaState, -2); // env's metatable = metatable

	// Reads not found in env fall through to the shared table; writes only ever land in env.
	lua_rawseti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}

void ScriptAction::__captureLastNodeEnv()
{
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]

	if(_lastNodeEnvRef) luaL_unref(_luaState, LUA_REGISTRYINDEX, _lastNodeEnvRef);

	_lastNodeEnvRef = luaL_ref(_luaState, LUA_REGISTRYINDEX); // [ ]
}

void* ScriptAction::__alloc(void* userData, void* ptr, size_t oldSize, size_t newSize)
{
	ScriptAction* action = static_cast<ScriptAction*>(userData);

	if(newSize == 0)
	{
		if(ptr) action -> _memoryUsed -= oldSize;
		free(ptr);
		return 0;
	}

	size_t usedWithoutThisBlock = action -> _memoryUsed - (ptr ? oldSize : 0);

	if(usedWithoutThisBlock + newSize > MEMORY_LIMIT) return 0;

	void* newPtr = realloc(ptr, newSize);

	if(newPtr) action -> _memoryUsed = usedWithoutThisBlock + newSize;

	return newPtr;
}

int ScriptAction::__safeLoad(lua_State* luaState)
{
	size_t len = 0;
	const char* chunk = luaL_checklstring(luaState, 1, &len);
	const char* chunkName = luaL_optstring(luaState, 2, chunk);

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(luaState, chunk, len, chunkName, "t") != LUA_OK)
	{
		lua_pushnil(luaState);
		lua_insert(luaState, -2);
		return 2;
	}

	return 1;
}
