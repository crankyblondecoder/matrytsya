#include "ScriptNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphException.hpp"
#include "../GraphPoke.hpp"

#include "../../lua/lua.hpp"

#include <cmath>
#include <cstdlib>

namespace
{
	// Globals the base library opens with that reach outside the sandbox (filesystem or stdio); stripped once
	// each state is created.
	const char* const DISALLOWED_GLOBALS[] = {"dofile", "loadfile", "print", "warn"};
}

ScriptNode::~ScriptNode()
{
	if(_coreLuaState) lua_close(_coreLuaState);
	if(_pokeLuaState) lua_close(_pokeLuaState);
}

ScriptNode::ScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: GraphNode(), _coreScript{coreScript}, _pokeScript(pokeScript)
{
	_setEnergyCost(1);

	// Supports script action.
	_addActionFlag(SCRIPT_GRAPH_ACTION);

	__compileCoreScript();
	__compilePokeScript();

	_coreLuaState = __createSandboxedState(&_coreMemoryUsed, &_coreBaseEnvRef);
	_pokeLuaState = __createSandboxedState(&_pokeMemoryUsed, &_pokeBaseEnvRef);

	// Prime both states with a fresh environment up front, so setGlobal()/_registerCoreGlobals() have a
	// live fresh table to write into even before the first invoke()/poke.
	__installFreshEnv(_coreLuaState, _coreBaseEnvRef);
	__installFreshEnv(_pokeLuaState, _pokeBaseEnvRef);
}

bool ScriptNode::invoke()
{
	if(_coreBytecode.empty()) return false;

	__registerCoreGlobalsOnce();

	bool success = luaL_loadbufferx(_coreLuaState, _coreBytecode.data(), _coreBytecode.size(), "script", "b") == LUA_OK;

	// Mode "b" only accepts bytecode. _coreBytecode is compiled from _coreScript once, at construction, by
	// this class itself rather than supplied by the script being run, so it never crosses the trust boundary
	// that the "t"-only loading elsewhere in this module guards against.
	if(success)
	{
		success = lua_pcall(_coreLuaState, 0, 0, 0) == LUA_OK;
	}
	else
	{
		lua_pop(_coreLuaState, 1);
	}

	// Note: lua_pcall already pops the function and any error message off the stack on failure, unlike
	// luaL_loadbufferx, which leaves an error message on the stack that has to be popped explicitly.

	// Capture the environment the script just ran against before replacing it, so getGlobal() can still
	// read back whatever it set; then immediately re-prime a fresh environment for the next invoke() (or
	// for ScriptAction::setGlobal() calls that happen before it).
	__captureEnv(_coreLuaState, &_corePostInvokeEnvRef);
	__installFreshEnv(_coreLuaState, _coreBaseEnvRef);

	return success;
}

void ScriptNode::setGlobal(const char* name, bool value)
{
	lua_pushboolean(_coreLuaState, value);
	lua_setglobal(_coreLuaState, name);
}

void ScriptNode::setGlobal(const char* name, int value)
{
	lua_pushinteger(_coreLuaState, value);
	lua_setglobal(_coreLuaState, name);
}

void ScriptNode::setGlobal(const char* name, double value)
{
	lua_pushnumber(_coreLuaState, value);
	lua_setglobal(_coreLuaState, name);
}

void ScriptNode::setGlobal(const char* name, const char* value)
{
	lua_pushstring(_coreLuaState, value);
	lua_setglobal(_coreLuaState, name);
}

bool ScriptNode::getGlobal(const char* name, bool& value)
{
	int envRef = _corePostInvokeEnvRef ? _corePostInvokeEnvRef : _coreBaseEnvRef;

	lua_rawgeti(_coreLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_coreLuaState, -1, name); // [env, value]

	bool found = lua_isboolean(_coreLuaState, -1);
	if(found) value = lua_toboolean(_coreLuaState, -1);

	lua_pop(_coreLuaState, 2);
	return found;
}

bool ScriptNode::getGlobal(const char* name, int& value)
{
	int envRef = _corePostInvokeEnvRef ? _corePostInvokeEnvRef : _coreBaseEnvRef;

	lua_rawgeti(_coreLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_coreLuaState, -1, name); // [env, value]

	bool found = lua_isinteger(_coreLuaState, -1);
	if(found) value = static_cast<int>(lua_tointeger(_coreLuaState, -1));

	lua_pop(_coreLuaState, 2);
	return found;
}

bool ScriptNode::getGlobal(const char* name, double& value)
{
	int envRef = _corePostInvokeEnvRef ? _corePostInvokeEnvRef : _coreBaseEnvRef;

	lua_rawgeti(_coreLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_coreLuaState, -1, name); // [env, value]

	bool found = lua_isnumber(_coreLuaState, -1);
	if(found) value = lua_tonumber(_coreLuaState, -1);

	lua_pop(_coreLuaState, 2);
	return found;
}

bool ScriptNode::getGlobal(const char* name, const char*& value)
{
	int envRef = _corePostInvokeEnvRef ? _corePostInvokeEnvRef : _coreBaseEnvRef;

	lua_rawgeti(_coreLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_coreLuaState, -1, name); // [env, value]

	bool found = lua_isstring(_coreLuaState, -1);
	if(found) value = lua_tostring(_coreLuaState, -1);

	// Note: the string pointer returned by lua_tostring() is anchored by the value left on the stack, which
	// is in turn anchored by the env table (still referenced from the registry), so it remains valid until
	// that table's "name" field is overwritten or the table itself is replaced/unreferenced.
	lua_pop(_coreLuaState, 2);
	return found;
}

bool ScriptNode::getPokeGlobal(const char* name, bool& value)
{
	int envRef = _pokePostInvokeEnvRef ? _pokePostInvokeEnvRef : _pokeBaseEnvRef;

	lua_rawgeti(_pokeLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_pokeLuaState, -1, name); // [env, value]

	bool found = lua_isboolean(_pokeLuaState, -1);
	if(found) value = lua_toboolean(_pokeLuaState, -1);

	lua_pop(_pokeLuaState, 2);
	return found;
}

bool ScriptNode::getPokeGlobal(const char* name, int& value)
{
	int envRef = _pokePostInvokeEnvRef ? _pokePostInvokeEnvRef : _pokeBaseEnvRef;

	lua_rawgeti(_pokeLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_pokeLuaState, -1, name); // [env, value]

	bool found = lua_isinteger(_pokeLuaState, -1);
	if(found) value = static_cast<int>(lua_tointeger(_pokeLuaState, -1));

	lua_pop(_pokeLuaState, 2);
	return found;
}

bool ScriptNode::getPokeGlobal(const char* name, double& value)
{
	int envRef = _pokePostInvokeEnvRef ? _pokePostInvokeEnvRef : _pokeBaseEnvRef;

	lua_rawgeti(_pokeLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_pokeLuaState, -1, name); // [env, value]

	bool found = lua_isnumber(_pokeLuaState, -1);
	if(found) value = lua_tonumber(_pokeLuaState, -1);

	lua_pop(_pokeLuaState, 2);
	return found;
}

bool ScriptNode::getPokeGlobal(const char* name, const char*& value)
{
	int envRef = _pokePostInvokeEnvRef ? _pokePostInvokeEnvRef : _pokeBaseEnvRef;

	lua_rawgeti(_pokeLuaState, LUA_REGISTRYINDEX, envRef); // [env]
	lua_getfield(_pokeLuaState, -1, name); // [env, value]

	bool found = lua_isstring(_pokeLuaState, -1);
	if(found) value = lua_tostring(_pokeLuaState, -1);

	lua_pop(_pokeLuaState, 2);
	return found;
}

ScriptActionTarget* ScriptNode::getScriptActionTarget()
{
	return this;
}

void ScriptNode::_poked(GraphPoke poke)
{
	if(_pokeBytecode.empty()) return;

	__registerPokeGlobalsOnce();

	__exposePokeContext(_pokeLuaState, poke);

	if(luaL_loadbufferx(_pokeLuaState, _pokeBytecode.data(), _pokeBytecode.size(), "script", "b") == LUA_OK)
	{
		lua_pcall(_pokeLuaState, 0, 0, 0);
	}
	else
	{
		lua_pop(_pokeLuaState, 1);
	}

	__captureEnv(_pokeLuaState, &_pokePostInvokeEnvRef);
	__installFreshEnv(_pokeLuaState, _pokeBaseEnvRef);
}

void ScriptNode::__compileCoreScript()
{
	lua_State* scratchState = luaL_newstate();

	if(!scratchState) return;

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(scratchState, _coreScript.c_str(), _coreScript.size(), "script", "t") == LUA_OK)
	{
		// Strip debug info: invoke() never inspects line numbers or names from a failed pcall.
		lua_dump(scratchState, __writeBytecode, &_coreBytecode, 1);
	}

	lua_close(scratchState);
}

void ScriptNode::__compilePokeScript()
{
	lua_State* scratchState = luaL_newstate();

	if(!scratchState) return;

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(scratchState, _pokeScript.c_str(), _pokeScript.size(), "script", "t") == LUA_OK)
	{
		// Strip debug info: _poked() never inspects line numbers or names from a failed pcall.
		lua_dump(scratchState, __writeBytecode, &_pokeBytecode, 1);
	}

	lua_close(scratchState);
}

int ScriptNode::__writeBytecode(lua_State*, const void* data, size_t size, void* userData)
{
	static_cast<std::string*>(userData) -> append(static_cast<const char*>(data), size);
	return 0;
}

lua_State* ScriptNode::__createSandboxedState(size_t* memoryUsed, int* baseEnvRef)
{
	lua_State* luaState = lua_newstate(__alloc, memoryUsed, luaL_makeseed(0));

	if(!luaState) throw GraphException(GraphException::SCRIPT_STATE_BAD_ALLOC);

	// Only libraries with no filesystem, process, environment or introspection access are opened. io, os,
	// package and debug are deliberately left closed.
	luaL_openselectedlibs(luaState,
		LUA_GLIBK | LUA_COLIBK | LUA_MATHLIBK | LUA_STRLIBK | LUA_TABLIBK | LUA_UTF8LIBK, 0);

	for(const char* global : DISALLOWED_GLOBALS)
	{
		lua_pushnil(luaState);
		lua_setglobal(luaState, global);
	}

	lua_pushcfunction(luaState, __safeLoad);
	lua_setglobal(luaState, "load");

	// Keep this table around as the clean base every fresh per-invoke env reads through to; it is never
	// installed as the live global table again after this point.
	lua_pushglobaltable(luaState);
	*baseEnvRef = luaL_ref(luaState, LUA_REGISTRYINDEX);

	return luaState;
}

void ScriptNode::__installFreshEnv(lua_State* luaState, int baseEnvRef)
{
	lua_newtable(luaState); // env
	lua_newtable(luaState); // metatable
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, baseEnvRef); // base table
	lua_setfield(luaState, -2, "__index"); // metatable.__index = base table
	lua_setmetatable(luaState, -2); // env's metatable = metatable

	// Reads not found in env fall through to the base table; writes only ever land in env.
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}

void ScriptNode::__captureEnv(lua_State* luaState, int* postInvokeEnvRef)
{
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]

	if(*postInvokeEnvRef) luaL_unref(luaState, LUA_REGISTRYINDEX, *postInvokeEnvRef);

	*postInvokeEnvRef = luaL_ref(luaState, LUA_REGISTRYINDEX); // [ ]
}

void ScriptNode::__registerCoreGlobalsOnce()
{
	__registerGlobalsOnce(_coreLuaState, _coreBaseEnvRef, _coreGlobalsRegistered);
}

void ScriptNode::__registerPokeGlobalsOnce()
{
	__registerGlobalsOnce(_pokeLuaState, _pokeBaseEnvRef, _pokeGlobalsRegistered);
}

void ScriptNode::__registerGlobalsOnce(lua_State* luaState, int baseEnvRef, bool& registered)
{
	if(registered) return;

	// Save whatever is currently live as globals (e.g. globals just written by setGlobal() ahead of this
	// invoke()), so those writes aren't lost by temporarily swapping globals to the base table below.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [currentEnv]
	int currentEnvRef = luaL_ref(luaState, LUA_REGISTRYINDEX); // [ ]

	// Temporarily make the permanent base table live as globals, so _registerCoreGlobals()'s
	// lua_setglobal() calls land there instead of the current per-invoke table, and so they survive
	// every future __installFreshEnv() reset.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, baseEnvRef); // [base]
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [ ]

	_registerCoreGlobals(luaState);

	// Restore the env that was live before; its metatable already falls through to the base table, so it
	// picks up the newly registered bindings the same way it already picks up stdlib functions.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, currentEnvRef); // [currentEnv]
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [ ]
	luaL_unref(luaState, LUA_REGISTRYINDEX, currentEnvRef);

	registered = true;
}

void ScriptNode::__exposePokeContext(lua_State* luaState, GraphPoke poke)
{
	const char* typeName = "HIT";

	if(poke.getType() == GraphPoke::PokeType::GRAB) typeName = "GRAB";
	else if(poke.getType() == GraphPoke::PokeType::DRAG) typeName = "DRAG";

	lua_pushstring(luaState, typeName);
	lua_setglobal(luaState, "POKE_TYPE");

	lua_pushinteger(luaState, poke.getHitDuration());
	lua_setglobal(luaState, "HIT_DURATION");

	float dragVector[3];
	poke.getDragVector(dragVector);

	lua_createtable(luaState, 3, 0); // [dragVector]

	for(int i = 0; i < 3; i++)
	{
		lua_pushnumber(luaState, dragVector[i]);
		lua_seti(luaState, -2, i + 1); // [dragVector]
	}

	lua_setglobal(luaState, "DRAG_VECTOR");
}

void* ScriptNode::__alloc(void* userData, void* ptr, size_t oldSize, size_t newSize)
{
	size_t* memoryUsed = static_cast<size_t*>(userData);

	if(newSize == 0)
	{
		if(ptr) *memoryUsed -= oldSize;
		free(ptr);
		return 0;
	}

	size_t usedWithoutThisBlock = *memoryUsed - (ptr ? oldSize : 0);

	if(usedWithoutThisBlock + newSize > MEMORY_LIMIT) return 0;

	void* newPtr = realloc(ptr, newSize);

	if(newPtr) *memoryUsed = usedWithoutThisBlock + newSize;

	return newPtr;
}

int ScriptNode::__safeLoad(lua_State* luaState)
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

void ScriptNode::_readDoubleArray(lua_State* luaState, int tableIndex, const char* field, double* out, int count)
{
	lua_getfield(luaState, tableIndex, field); // [..., field]

	if(lua_istable(luaState, -1))
	{
		for(int i = 0; i < count; i++)
		{
			lua_geti(luaState, -1, i + 1); // [..., field, element]
			out[i] = lua_tonumber(luaState, -1);
			lua_pop(luaState, 1); // [..., field]
		}
	}

	lua_pop(luaState, 1); // [...]
}

void ScriptNode::_readByteArray(lua_State* luaState, int tableIndex, const char* field, std::byte* out, int count)
{
	lua_getfield(luaState, tableIndex, field); // [..., field]

	if(lua_istable(luaState, -1))
	{
		for(int i = 0; i < count; i++)
		{
			lua_geti(luaState, -1, i + 1); // [..., field, element]

			// lua_tointeger() only succeeds on values that are already exactly integral, returning 0 for
			// any other number (e.g. the fractional result of a colour lerp), so round instead.
			out[i] = static_cast<std::byte>(std::lround(lua_tonumber(luaState, -1)));

			lua_pop(luaState, 1); // [..., field]
		}
	}

	lua_pop(luaState, 1); // [...]
}
