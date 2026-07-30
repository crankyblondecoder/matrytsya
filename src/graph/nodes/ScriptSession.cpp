#include "ScriptSession.hpp"

#include "ScriptNode.hpp"
#include "../GraphException.hpp"

#include "../../lua/lua.hpp"

ScriptSession::~ScriptSession()
{
	// Frees the state's resource lock. Done here, in the body, so it happens before _node drops its reference
	// and potentially deletes the node the lock lives in.
	_node.getInstance() -> __releaseState(_poke);
}

ScriptSession::ScriptSession(ScriptNode* node, lua_State* luaState, bool poke)
	: _node(node), _luaState{luaState}, _poke{poke}
{
	// A session that could not reference its node would never be able to release the state lock it was
	// handed, so it must not come into existence at all. The requester holds a reference to the node across
	// the request, so this only fails if that contract is broken.
	if(!_node.isValid()) throw GraphException(GraphException::SCRIPT_SESSION_NODE_UNAVAILABLE);
}

bool ScriptSession::run()
{
	return _poke ? _node.getInstance() -> __runPoke() : _node.getInstance() -> __runCore();
}

void ScriptSession::setGlobal(const char* name, bool value)
{
	lua_pushboolean(_luaState, value);
	lua_setglobal(_luaState, name);
}

void ScriptSession::setGlobal(const char* name, int value)
{
	lua_pushinteger(_luaState, value);
	lua_setglobal(_luaState, name);
}

void ScriptSession::setGlobal(const char* name, double value)
{
	lua_pushnumber(_luaState, value);
	lua_setglobal(_luaState, name);
}

void ScriptSession::setGlobal(const char* name, const char* value)
{
	lua_pushstring(_luaState, value);
	lua_setglobal(_luaState, name);
}

void ScriptSession::setGlobal(const char* name, const float* values, int count)
{
	lua_createtable(_luaState, count, 0); // [table]

	for(int index = 0; index < count; index++)
	{
		lua_pushnumber(_luaState, values[index]); // [table, value]
		lua_seti(_luaState, -2, index + 1); // [table]
	}

	lua_setglobal(_luaState, name); // [ ]
}

bool ScriptSession::getGlobal(const char* name, bool& value)
{
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]
	lua_getfield(_luaState, -1, name); // [env, value]

	bool found = lua_isboolean(_luaState, -1);
	if(found) value = lua_toboolean(_luaState, -1);

	lua_pop(_luaState, 2);
	return found;
}

bool ScriptSession::getGlobal(const char* name, int& value)
{
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]
	lua_getfield(_luaState, -1, name); // [env, value]

	bool found = lua_isinteger(_luaState, -1);
	if(found) value = static_cast<int>(lua_tointeger(_luaState, -1));

	lua_pop(_luaState, 2);
	return found;
}

bool ScriptSession::getGlobal(const char* name, double& value)
{
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]
	lua_getfield(_luaState, -1, name); // [env, value]

	bool found = lua_isnumber(_luaState, -1);
	if(found) value = lua_tonumber(_luaState, -1);

	lua_pop(_luaState, 2);
	return found;
}

bool ScriptSession::getGlobal(const char* name, const char*& value)
{
	lua_rawgeti(_luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [env]
	lua_getfield(_luaState, -1, name); // [env, value]

	bool found = lua_isstring(_luaState, -1);
	if(found) value = lua_tostring(_luaState, -1);

	// Note: the string pointer returned by lua_tostring() is anchored by the value left on the stack, which
	// is in turn anchored by the env table (still referenced from the registry), so it remains valid until
	// that table's "name" field is overwritten or the table itself is replaced/unreferenced.
	lua_pop(_luaState, 2);
	return found;
}
