#include "SceneTransformNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

#include "../../lua/lua.hpp"

SceneTransformNode::~SceneTransformNode()
{
}

SceneTransformNode::SceneTransformNode(const std::string& script, const std::string& pokeScript)
	: ScriptNode(script, pokeScript)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

void SceneTransformNode::setTransform(Transform transform)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];
}

bool SceneTransformNode::invoke(lua_State* luaState)
{
	__registerTransformBindings(luaState);

	return ScriptNode::invoke(luaState);
}

void SceneTransformNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	if(surface.isValid()) surface.getInstance() -> addLocalTransform(_transform, getId());
}

void SceneTransformNode::strobe()
{
}

SceneActionTarget* SceneTransformNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneTransformNode::getStrobeActionTarget()
{
	return this;
}

int SceneTransformNode::__luaGetTransform(lua_State* luaState)
{
	SceneTransformNode* node = static_cast<SceneTransformNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_createtable(luaState, 16, 0); // [transform]

	for(int i = 0; i < 16; i++)
	{
		lua_pushnumber(luaState, node -> _transform[i]);
		lua_seti(luaState, -2, i + 1); // [transform]
	}

	return 1;
}

int SceneTransformNode::__luaSetTransform(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTransformNode* node = static_cast<SceneTransformNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	for(int i = 0; i < 16; i++)
	{
		lua_geti(luaState, 1, i + 1); // [transform, element]
		node -> _transform[i] = lua_tonumber(luaState, -1);
		lua_pop(luaState, 1); // [transform]
	}

	return 0;
}

void SceneTransformNode::__registerTransformBindings(lua_State* luaState)
{
	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaGetTransform, 1);
	lua_setglobal(luaState, "getTransform");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaSetTransform, 1);
	lua_setglobal(luaState, "setTransform");
}
