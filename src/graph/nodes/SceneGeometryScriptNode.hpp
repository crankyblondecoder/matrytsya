#ifndef SCENE_GEOMETRY_SCRIPT_NODE_H
#define SCENE_GEOMETRY_SCRIPT_NODE_H

#include <string>

#include "../actionTargets/AgentVisibleActionTarget.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "../GraphFocusable.hpp"
#include "AnimateScriptNode.hpp"
#include "SceneGeometry.hpp"

/**
 * Graph node that represents scene geometry.
 */
class SceneGeometryScriptNode : public AnimateScriptNode, public SceneActionTarget,
	public AgentVisibleActionTarget, public GraphFocusable, public SceneGeometry
{
    public:

		/**
		 * @param coreScript Lua source code that this node runs when invoked.
		 * @param pokeScript Lua source code that this node runs when poked.
		 */
        SceneGeometryScriptNode(const std::string& coreScript, const std::string& pokeScript);

		Type getType() override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;

		AgentVisibleActionTarget* getAgentVisibleActionTarget() override;

		void setAgentVisible(bool flag) override;

		bool getAgentVisible() override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
        virtual ~SceneGeometryScriptNode();

		void _registerCoreGlobals(lua_State* luaState) override;

    private:

        // Do not allow copying.
        SceneGeometryScriptNode(const SceneGeometryScriptNode& copyFrom);
        SceneGeometryScriptNode& operator= (const SceneGeometryScriptNode& copyFrom);

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
		 * Read an optional VertexVisibility argument, supplied from Lua as one of the VertexVisibility.*
		 * global constants, defaulting to ALWAYS when the argument is absent.
		 * @param luaState Lua state the call is running against.
		 * @param index Stack index of the (optional) visibility argument.
		 * @returns The VertexVisibility the argument names, or ALWAYS if it was omitted.
		 * @throw Raises a Lua error (does not return) if the argument is present but not a known
		 *        VertexVisibility value.
		 */
		static SceneGeometry::VertexVisibility __checkVisibility(lua_State* luaState, int index);

		/**
		 * Lua-facing `addVertex(vertex, [visibility])`: appends a Vertex built by __luaVertexConstructor()
		 * to the node bound as this closure's upvalue.
		 * @param luaState Lua state the call is running against; argument 1 is the Vertex userdata,
		 *        argument 2 is an optional VertexVisibility.* constant (default ALWAYS) and upvalue 1 is a
		 *        light userdata pointing at the target SceneGeometryScriptNode.
		 * @returns Always 0.
		 */
		static int __luaAddVertex(lua_State* luaState);

		/**
		 * Lua-facing `addVertexes(vertexes, [visibility])`: appends every Vertex in the given array-style
		 * table to the node bound as this closure's upvalue, in a single call.
		 * @param luaState Lua state the call is running against; argument 1 is a table of Vertex userdata
		 *        (indexes 1..#vertexes), argument 2 is an optional VertexVisibility.* constant (default
		 *        ALWAYS) and upvalue 1 is a light userdata pointing at the target SceneGeometryScriptNode.
		 * @returns Always 0.
		 */
		static int __luaAddVertexes(lua_State* luaState);

		/**
		 * Lua-facing `vertexCount()`: returns the number of vertexes currently held by the node bound as
		 * this closure's upvalue.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target SceneGeometryScriptNode.
		 * @returns Always 1 (the vertex count is left on the stack).
		 */
		static int __luaVertexCount(lua_State* luaState);

		/**
		 * Lua-facing `setAgentVisible(visible)`: sets whether the node bound as this closure's upvalue should
		 * currently show its VertexVisibility.AGENT vertexes.
		 * @param luaState Lua state the call is running against; argument 1 is the boolean to set and upvalue
		 *        1 is a light userdata pointing at the target SceneGeometryScriptNode.
		 * @returns Always 0.
		 */
		static int __luaSetAgentVisible(lua_State* luaState);

		/**
		 * Lua-facing `getAgentVisible()`: returns whether the node bound as this closure's upvalue is
		 * currently showing its VertexVisibility.AGENT vertexes.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target SceneGeometryScriptNode.
		 * @returns Always 1 (the flag is left on the stack).
		 */
		static int __luaGetAgentVisible(lua_State* luaState);

		/**
		 * Register the `Vertex` constructor, the `VertexVisibility` constants table and the
		 * `addVertex()`/`addVertexes()`/`vertexCount()`/`setAgentVisible()`/`getAgentVisible()` global
		 * functions against luaState, binding each function to this node instance via upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerVertexBindings(lua_State* luaState);

		/// Metatable name used to type-check Vertex userdata passed into addVertex().
		static constexpr const char* VERTEX_METATABLE = "SceneGeometryScriptNode.Vertex";
};

#endif
