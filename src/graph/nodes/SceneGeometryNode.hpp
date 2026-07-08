#ifndef SCENE_GEOMETRY_NODE_H
#define SCENE_GEOMETRY_NODE_H

#include <string>
#include <vector>

#include "../graphSceneElements.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/SceneStrobeActionTarget.hpp"
#include "ScriptNode.hpp"

/**
 * Graph node that represents scene geometry.
 */
class SceneGeometryNode : public ScriptNode, public SceneActionTarget, public SceneStrobeActionTarget
{
    public:

        virtual ~SceneGeometryNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 */
        SceneGeometryNode(const std::string& script);

		/**
		 * Add vertexes to the list of vertexes for this scene node.
		 */
		void addVertexes(std::vector<Vertex> vertexesToAdd);

		bool invoke(lua_State* luaState) override;

		void populateSurface(GraphHiveSceneSurface& surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;
		SceneStrobeActionTarget* getSceneStrobeActionTarget() override;

	protected:

    private:

        // Do not allow copying.
        SceneGeometryNode(const SceneGeometryNode& copyFrom);
        SceneGeometryNode& operator= (const SceneGeometryNode& copyFrom);

		/**
		 * Lua-facing constructor for the Vertex type: `Vertex{posn = {...}, colour = {...}, texCoords = {...},
		 * normal = {...}}`. Builds a Vertex userdata from the fields present in the table argument, leaving
		 * any field that is absent zeroed.
		 * @param luaState Lua state the call is running against; argument 1 is the field table.
		 * @returns Always 1 (the constructed Vertex userdata is left on the stack).
		 */
		static int __luaVertexConstructor(lua_State* luaState);

		/**
		 * Type-check a Vertex userdata argument and copy out the Vertex it holds.
		 * @param luaState Lua state the call is running against.
		 * @param index Stack index of the Vertex userdata argument.
		 * @returns The Vertex the userdata at that index holds.
		 */
		static Vertex __checkVertex(lua_State* luaState, int index);

		/**
		 * Lua-facing `addVertex(vertex)`: appends a Vertex built by __luaVertexConstructor() to the node
		 * bound as this closure's upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is the Vertex userdata and
		 *        upvalue 1 is a light userdata pointing at the target SceneGeometryNode.
		 * @returns Always 0.
		 */
		static int __luaAddVertex(lua_State* luaState);

		/**
		 * Lua-facing `addVertexes(vertexes)`: appends every Vertex in the given array-style table to the
		 * node bound as this closure's upvalue, in a single call.
		 * @param luaState Lua state the call is running against; argument 1 is a table of Vertex userdata
		 *        (indexes 1..#vertexes) and upvalue 1 is a light userdata pointing at the target
		 *        SceneGeometryNode.
		 * @returns Always 0.
		 */
		static int __luaAddVertexes(lua_State* luaState);

		/**
		 * Lua-facing `vertexCount()`: returns the number of vertexes currently held by the node bound as
		 * this closure's upvalue.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target SceneGeometryNode.
		 * @returns Always 1 (the vertex count is left on the stack).
		 */
		static int __luaVertexCount(lua_State* luaState);

		/**
		 * Register the `Vertex` constructor and `addVertex()`/`addVertexes()`/`vertexCount()` global
		 * functions against luaState, binding each to this node instance via upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerVertexBindings(lua_State* luaState);

		/// Metatable name used to type-check Vertex userdata passed into addVertex().
		static constexpr const char* VERTEX_METATABLE = "SceneGeometryNode.Vertex";

		/**
		 * The vertexes that make up the scene object this node defines.
		 * Each triplet defines a triangle with standard counter-clockwise winding order for the front face.
		 * @note There is no indexing at this stage.
		 */
		std::vector<Vertex> _vertexes;
};

#endif
