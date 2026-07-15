#include "StrobeScriptNode.hpp"

#include "../../lua/lua.hpp"

StrobeScriptNode::~StrobeScriptNode()
{
}

StrobeScriptNode::StrobeScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: ScriptNode(coreScript, pokeScript)
{
}

void StrobeScriptNode::setStrobe(bool flag)
{
	_strobe = flag;
}

StrobeActionTarget* StrobeScriptNode::getStrobeActionTarget()
{
	return this;
}

void StrobeScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	__registerStrobeBindings(luaState);
}

int StrobeScriptNode::__luaGetStrobe(lua_State* luaState)
{
	StrobeScriptNode* node = static_cast<StrobeScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushboolean(luaState, node -> _strobe);

	return 1;
}

void StrobeScriptNode::__registerStrobeBindings(lua_State* luaState)
{
	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaGetStrobe, 1);
	lua_setglobal(luaState, "getStrobe");
}
