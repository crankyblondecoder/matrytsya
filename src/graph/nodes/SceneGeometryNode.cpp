#include "SceneGeometryNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

#include "../../lua/lua.hpp"

SceneGeometryNode::~SceneGeometryNode()
{
}

SceneGeometryNode::SceneGeometryNode(const std::string& coreScript, const std::string& pokeScript)
	: StrobeScriptNode(coreScript, pokeScript)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

void SceneGeometryNode::addVertexes(std::vector<Vertex> vertexesToAdd)
{
	_vertexes.insert(_vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
}

void SceneGeometryNode::addVertexes(double* rawData, unsigned length)
{
	for(unsigned index = 0; index + VERTEX_SERIAL_SIZE <= length;)
	{
		// Pack into a Vertex.
		Vertex newVertex {

			// Position: X, Y, Z
			{rawData[index++], rawData[index++], rawData[index++]},

			// Colour: R, G, B, A
			//std::byte colour[4];
			{

				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++]),
				static_cast<std::byte>(rawData[index++])
			},

			// Texture coordinates: U, V
			// double texCoords[2];
			{rawData[index++], rawData[index++]},

			// Normal (must be normalised): X, Y, Z
			//double normal[3];
			{rawData[index++], rawData[index++], rawData[index++]}
		};

		_vertexes.push_back(newVertex);
	}
}

void SceneGeometryNode::_registerCoreGlobals(lua_State* luaState)
{
	StrobeScriptNode::_registerCoreGlobals(luaState);

	__registerVertexBindings(luaState);
	__registerAnimatingBindings(luaState);
}

void SceneGeometryNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	if(surface.isValid()) surface.getInstance() -> addVertexes(_vertexes, getId(), getId(), getPokeEnabled());
}

void SceneGeometryNode::strobe()
{
}

SceneActionTarget* SceneGeometryNode::getSceneActionTarget()
{
	return this;
}

int SceneGeometryNode::__luaVertexConstructor(lua_State* luaState)
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

Vertex SceneGeometryNode::__checkVertex(lua_State* luaState, int index)
{
	return *static_cast<Vertex*>(luaL_checkudata(luaState, index, VERTEX_METATABLE));
}

int SceneGeometryNode::__luaAddVertex(lua_State* luaState)
{
	Vertex vertex = __checkVertex(luaState, 1);
	SceneGeometryNode* node = static_cast<SceneGeometryNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	node -> addVertexes({vertex});

	return 0;
}

int SceneGeometryNode::__luaAddVertexes(lua_State* luaState)
{
	luaL_checktype(luaState, 1, LUA_TTABLE);

	SceneGeometryNode* node = static_cast<SceneGeometryNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_Integer count = luaL_len(luaState, 1);

	std::vector<Vertex> vertexesToAdd;
	vertexesToAdd.reserve(count > 0 ? static_cast<size_t>(count) : 0);

	for(lua_Integer i = 1; i <= count; i++)
	{
		lua_geti(luaState, 1, i); // [..., vertexes, vertex]
		vertexesToAdd.push_back(__checkVertex(luaState, -1));
		lua_pop(luaState, 1); // [..., vertexes]
	}

	node -> addVertexes(vertexesToAdd);

	return 0;
}

int SceneGeometryNode::__luaVertexCount(lua_State* luaState)
{
	SceneGeometryNode* node = static_cast<SceneGeometryNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushinteger(luaState, static_cast<lua_Integer>(node -> _vertexes.size()));

	return 1;
}

void SceneGeometryNode::__registerVertexBindings(lua_State* luaState)
{
	// Only creates the metatable the first time it is seen by this lua_State; a no-op on later calls.
	luaL_newmetatable(luaState, VERTEX_METATABLE);
	lua_pop(luaState, 1);

	lua_pushcfunction(luaState, __luaVertexConstructor);
	lua_setglobal(luaState, "Vertex");

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

int SceneGeometryNode::__luaGetAnimating(lua_State* luaState)
{
	SceneGeometryNode* node = static_cast<SceneGeometryNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	lua_pushboolean(luaState, node -> _animating);

	return 1;
}

int SceneGeometryNode::__luaSetAnimating(lua_State* luaState)
{
	SceneGeometryNode* node = static_cast<SceneGeometryNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	node -> _animating = lua_toboolean(luaState, 1);

	return 0;
}

void SceneGeometryNode::__registerAnimatingBindings(lua_State* luaState)
{
	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaGetAnimating, 1);
	lua_setglobal(luaState, "getAnimating");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaSetAnimating, 1);
	lua_setglobal(luaState, "setAnimating");
}

