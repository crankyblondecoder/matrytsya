#include "ScriptNode.hpp"

#include "../graphActionFlagRegister.hpp"

#include "../../lua/lua.hpp"

ScriptNode::~ScriptNode()
{
}

ScriptNode::ScriptNode(const std::string& script) : GraphNode(), _script{script}
{
	_setEnergyCost(1);

	// Supports script action.
	_addActionFlag(SCRIPT_GRAPH_ACTION);
}

bool ScriptNode::invoke(lua_State* luaState)
{
	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(luaState, _script.c_str(), _script.size(), "script", "t") != LUA_OK)
	{
		lua_pop(luaState, 1);
		return false;
	}

	if(lua_pcall(luaState, 0, 0, 0) != LUA_OK)
	{
		lua_pop(luaState, 1);
		return false;
	}

	return true;
}

ScriptActionTarget* ScriptNode::getScriptActionTarget()
{
	return this;
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
