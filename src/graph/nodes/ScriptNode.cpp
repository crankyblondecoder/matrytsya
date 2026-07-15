#include "ScriptNode.hpp"

#include "../graphActionFlagRegister.hpp"

#include "../../lua/lua.hpp"

#include <cmath>

ScriptNode::~ScriptNode()
{
}

ScriptNode::ScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: GraphNode(), _coreScript{coreScript}, _pokeScript(pokeScript)
{
	_setEnergyCost(1);

	// Supports script action.
	_addActionFlag(SCRIPT_GRAPH_ACTION);

	__compileCoreScript();
}

bool ScriptNode::invoke(lua_State* luaState)
{
	if(_coreBytecode.empty()) return false;

	// Mode "b" only accepts bytecode. _coreBytecode is compiled from _coreScript once, at construction, by
	// this class itself rather than supplied by the script being run, so it never crosses the trust boundary
	// that the "t"-only loading elsewhere in this module guards against.
	if(luaL_loadbufferx(luaState, _coreBytecode.data(), _coreBytecode.size(), "script", "b") != LUA_OK)
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

void ScriptNode::_poked(GraphPoke poke)
{
	// TODO: run _pokeScript against a Lua state.
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

int ScriptNode::__writeBytecode(lua_State*, const void* data, size_t size, void* userData)
{
	static_cast<std::string*>(userData) -> append(static_cast<const char*>(data), size);
	return 0;
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
