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

void SceneTransformScriptNode::setTransform(const Transform transform)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];
}

void SceneTransformScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	AnimateScriptNode::_registerCoreGlobals(luaState);

	__registerTransformBindings(luaState);
}

void SceneTransformScriptNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	if(surface.isValid()) surface.getInstance() -> addLocalTransform(_transform, getId());
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

	lua_createtable(luaState, 16, 0); // [transform]

	for(int i = 0; i < 16; i++)
	{
		lua_pushnumber(luaState, node -> _transform[i]);
		lua_seti(luaState, -2, i + 1); // [transform]
	}

	return 1;
}

int SceneTransformScriptNode::__luaSetTransform(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneTransformScriptNode* node = static_cast<SceneTransformScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	for(int i = 0; i < 16; i++)
	{
		lua_geti(luaState, 1, i + 1); // [transform, element]
		node -> _transform[i] = lua_tonumber(luaState, -1);
		lua_pop(luaState, 1); // [transform]
	}

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
