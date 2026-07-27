#include "SceneTransformScriptNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

#include "../../lua/lua.hpp"

SceneTransformScriptNode::~SceneTransformScriptNode()
{
}

SceneTransformScriptNode::SceneTransformScriptNode(const std::string& script, const std::string& pokeScript)
	: AnimateScriptNode(script, pokeScript)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

GraphNode::Type SceneTransformScriptNode::getType()
{
	return Type::SCENE_TRANSFORM_SCRIPT_NODE;
}

void SceneTransformScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	AnimateScriptNode::_registerCoreGlobals(luaState);

	__registerTransformBindings(luaState);
}

void SceneTransformScriptNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	SceneTransform::populateSurface(surface, getId());
}

void SceneTransformScriptNode::strobe()
{
}

SceneActionTarget* SceneTransformScriptNode::getSceneActionTarget()
{
	return this;
}

int SceneTransformScriptNode::__luaGetTransform(lua_State* luaState)
{
	SceneTransformScriptNode* node = static_cast<SceneTransformScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	Transform transform;

	node -> getTransform(transform);

	lua_createtable(luaState, 16, 0); // [transform]

	for(int i = 0; i < 16; i++)
	{
		lua_pushnumber(luaState, transform[i]);
		lua_seti(luaState, -2, i + 1); // [transform]
	}

	return 1;
}

int SceneTransformScriptNode::__luaSetTransform(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTransformScriptNode* node = static_cast<SceneTransformScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	Transform transform;

	for(int i = 0; i < 16; i++)
	{
		lua_geti(luaState, 1, i + 1); // [transform, element]
		transform[i] = lua_tonumber(luaState, -1);
		lua_pop(luaState, 1); // [transform]
	}

	node -> setTransform(transform);

	return 0;
}

void SceneTransformScriptNode::__registerTransformBindings(lua_State* luaState)
{
	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaGetTransform, 1);
	lua_setglobal(luaState, "getTransform");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaSetTransform, 1);
	lua_setglobal(luaState, "setTransform");
}

unsigned SceneTransformScriptNode::getVersion()
{
	return GraphVersioned::getVersion();
}
