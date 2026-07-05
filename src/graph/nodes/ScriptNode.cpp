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
