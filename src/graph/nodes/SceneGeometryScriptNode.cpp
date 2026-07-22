#include "SceneGeometryScriptNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

#include "../../lua/lua.hpp"

SceneGeometryScriptNode::~SceneGeometryScriptNode()
{
}

SceneGeometryScriptNode::SceneGeometryScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: AnimateScriptNode(coreScript, pokeScript)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

void SceneGeometryScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	AnimateScriptNode::_registerCoreGlobals(luaState);

	__registerVertexBindings(luaState);
}

void SceneGeometryScriptNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	SceneGeometry::populateSurface(surface, getId(), getPokeEnabled());
}

void SceneGeometryScriptNode::strobe()
{
}

SceneActionTarget* SceneGeometryScriptNode::getSceneActionTarget()
{
	return this;
}

int SceneGeometryScriptNode::__luaVertexConstructor(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	Vertex* vertex = static_cast<Vertex*>(lua_newuserdatauv(luaState, sizeof(Vertex), 0));
	*vertex = Vertex{};

	_readDoubleArray(luaState, 1, "posn", vertex -> posn, 3);
	_readByteArray(luaState, 1, "colour", vertex -> colour, 4);
	_readDoubleArray(luaState, 1, "texCoords", vertex -> texCoords, 2);
	_readDoubleArray(luaState, 1, "normal", vertex -> normal, 3);

	luaL_setmetatable(luaState, VERTEX_METATABLE);

	return 1;
}

Vertex SceneGeometryScriptNode::__checkVertex(lua_State* luaState, int index)
{
	return *static_cast<Vertex*>(luaL_checkudata(luaState, index, VERTEX_METATABLE));
}

SceneGeometry::VertexVisibility SceneGeometryScriptNode::__checkVisibility(lua_State* luaState, int index)
{
	lua_Integer value = luaL_optinteger(luaState, index,
		static_cast<lua_Integer>(SceneGeometry::VertexVisibility::ALWAYS));

	switch(static_cast<SceneGeometry::VertexVisibility>(value))
	{
		case SceneGeometry::VertexVisibility::ALWAYS:
		case SceneGeometry::VertexVisibility::GRABBED:
		case SceneGeometry::VertexVisibility::DRAGGING:
		case SceneGeometry::VertexVisibility::HOVERED_OVER:
			return static_cast<SceneGeometry::VertexVisibility>(value);
	}

	// luaL_error does not return; the cast below is only present to satisfy the compiler.
	luaL_error(luaState, "invalid VertexVisibility value: %d", static_cast<int>(value));
	return SceneGeometry::VertexVisibility::ALWAYS;
}

int SceneGeometryScriptNode::__luaAddVertex(lua_State* luaState)
{
	Vertex vertex = __checkVertex(luaState, 1);
	SceneGeometry::VertexVisibility visibility = __checkVisibility(luaState, 2);
	SceneGeometryScriptNode* node = static_cast<SceneGeometryScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	node -> addVertexes({vertex}, visibility);

	return 0;
}

int SceneGeometryScriptNode::__luaAddVertexes(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneGeometry::VertexVisibility visibility = __checkVisibility(luaState, 2);
	SceneGeometryScriptNode* node = static_cast<SceneGeometryScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_Integer count = luaL_len(luaState, 1);

	std::vector<Vertex> vertexesToAdd;
	vertexesToAdd.reserve(count > 0 ? static_cast<size_t>(count) : 0);

	for(lua_Integer i = 1; i <= count; i++)
	{
		lua_geti(luaState, 1, i); // [..., vertexes, vertex]
		vertexesToAdd.push_back(__checkVertex(luaState, -1));
		lua_pop(luaState, 1); // [..., vertexes]
	}

	node -> addVertexes(vertexesToAdd, visibility);

	return 0;
}

int SceneGeometryScriptNode::__luaVertexCount(lua_State* luaState)
{
	SceneGeometryScriptNode* node = static_cast<SceneGeometryScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushinteger(luaState, static_cast<lua_Integer>(node -> getVertexCount()));

	return 1;
}

void SceneGeometryScriptNode::__registerVertexBindings(lua_State* luaState)
{
	// Only creates the metatable the first time it is seen by this lua_State; a no-op on later calls.
	luaL_newmetatable(luaState, VERTEX_METATABLE);
	lua_pop(luaState, 1);

	lua_pushcfunction(luaState, __luaVertexConstructor);
	lua_setglobal(luaState, "Vertex");

	// Expose the VertexVisibility enum as a global table of integer constants that round-trip through
	// __checkVisibility(); the values must match static_cast<lua_Integer> of each enum member.
	lua_createtable(luaState, 0, 4);

	lua_pushinteger(luaState, static_cast<lua_Integer>(SceneGeometry::VertexVisibility::ALWAYS));
	lua_setfield(luaState, -2, "ALWAYS");

	lua_pushinteger(luaState, static_cast<lua_Integer>(SceneGeometry::VertexVisibility::GRABBED));
	lua_setfield(luaState, -2, "GRABBED");

	lua_pushinteger(luaState, static_cast<lua_Integer>(SceneGeometry::VertexVisibility::DRAGGING));
	lua_setfield(luaState, -2, "DRAGGING");

	lua_pushinteger(luaState, static_cast<lua_Integer>(SceneGeometry::VertexVisibility::HOVERED_OVER));
	lua_setfield(luaState, -2, "HOVERED_OVER");

	lua_setglobal(luaState, "VertexVisibility");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaAddVertex, 1);
	lua_setglobal(luaState, "addVertex");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaAddVertexes, 1);
	lua_setglobal(luaState, "addVertexes");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaVertexCount, 1);
	lua_setglobal(luaState, "vertexCount");
}

unsigned SceneGeometryScriptNode::getVersion()
{
	return GraphVersioned::getVersion();
}
