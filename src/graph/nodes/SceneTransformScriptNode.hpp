#ifndef SCENE_TRANSFORM_SCRIPT_NODE_H
#define SCENE_TRANSFORM_SCRIPT_NODE_H

#include <string>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../graphSceneElements.hpp"
#include "AnimateScriptNode.hpp"

class GraphHiveSceneSurface;

struct lua_State;

/**
 * Graph node that represents a transform applied to scene geometry.
 * Its script can read and modify the current transform via the getTransform()/setTransform() Lua globals.
 */
class SceneTransformScriptNode : public AnimateScriptNode, public SceneActionTarget
{
    public:

        virtual ~SceneTransformScriptNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 * @param pokeScript Lua source code that this node runs when poked.
		 */
        SceneTransformScriptNode(const std::string& script, const std::string& pokeScript);

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 */
		void setTransform(Transform transform);

		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;

	protected:

		void _registerCoreGlobals(lua_State* luaState) override;

    private:

        // Do not allow copying.
        SceneTransformScriptNode(const SceneTransformScriptNode& copyFrom);
        SceneTransformScriptNode& operator= (const SceneTransformScriptNode& copyFrom);

		/**
		 * Lua-facing `getTransform()`: returns the current transform of the node bound as this closure's
		 * upvalue as a 16 element array table, in column-major order.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target SceneTransformScriptNode.
		 * @returns Always 1 (the constructed array table is left on the stack).
		 */
		static int __luaGetTransform(lua_State* luaState);

		/**
		 * Lua-facing `setTransform(transform)`: sets the transform of the node bound as this closure's
		 * upvalue from a 16 element array table, in column-major order.
		 * @param luaState Lua state the call is running against; argument 1 is the array table and upvalue 1
		 *        is a light userdata pointing at the target SceneTransformScriptNode.
		 * @returns Always 0.
		 */
		static int __luaSetTransform(lua_State* luaState);

		/**
		 * Register the `getTransform()`/`setTransform()` global functions against luaState, binding each to
		 * this node instance via upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerTransformBindings(lua_State* luaState);

		/**
		 * The local transform applied to the geometry.
		 * This is standard column-major order of matrix elements.
		 * Default is the identity matrix.
		 */
		Transform _transform = {

			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0
		};
};

#endif
