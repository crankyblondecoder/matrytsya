#ifndef SCENE_TRANSFORM_NODE_H
#define SCENE_TRANSFORM_NODE_H

#include <string>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/SceneStrobeActionTarget.hpp"
#include "../graphSceneElements.hpp"
#include "ScriptNode.hpp"

class GraphHiveSceneSurface;

struct lua_State;

/**
 * Graph node that represents a transform applied to scene geometry.
 * Its script can read and modify the current transform via the getTransform()/setTransform() Lua globals.
 */
class SceneTransformNode : public ScriptNode, public SceneActionTarget, public SceneStrobeActionTarget
{
    public:

        virtual ~SceneTransformNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 */
        SceneTransformNode(const std::string& script);

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 */
		void setTransform(Transform transform);

		bool invoke(lua_State* luaState) override;

		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;
		SceneStrobeActionTarget* getSceneStrobeActionTarget() override;

	protected:

    private:

        // Do not allow copying.
        SceneTransformNode(const SceneTransformNode& copyFrom);
        SceneTransformNode& operator= (const SceneTransformNode& copyFrom);

		/**
		 * Lua-facing `getTransform()`: returns the current transform of the node bound as this closure's
		 * upvalue as a 16 element array table, in column-major order.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target SceneTransformNode.
		 * @returns Always 1 (the constructed array table is left on the stack).
		 */
		static int __luaGetTransform(lua_State* luaState);

		/**
		 * Lua-facing `setTransform(transform)`: sets the transform of the node bound as this closure's
		 * upvalue from a 16 element array table, in column-major order.
		 * @param luaState Lua state the call is running against; argument 1 is the array table and upvalue 1
		 *        is a light userdata pointing at the target SceneTransformNode.
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
