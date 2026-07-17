#ifndef SCENE_TRANSFORM_NODE_H
#define SCENE_TRANSFORM_NODE_H

#include <atomic>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/StrobeActionTarget.hpp"
#include "../GraphNode.hpp"
#include "../graphSceneElements.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that represents a transform applied to scene geometry, set directly through its C++ API
 * rather than a Lua script.
 */
class SceneTransformNode : public GraphNode, public SceneActionTarget, public StrobeActionTarget
{
    public:

        SceneTransformNode();

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 */
		void setTransform(const Transform transform);

		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		void setStrobe(bool flag) override;

		SceneActionTarget* getSceneActionTarget() override;

		StrobeActionTarget* getStrobeActionTarget() override;

	protected:

		// Ref counted.
        virtual ~SceneTransformNode();

		void _poked(GraphPoke poke) override;

    private:

        // Do not allow copying.
        SceneTransformNode(const SceneTransformNode& copyFrom);
        SceneTransformNode& operator= (const SceneTransformNode& copyFrom);

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

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
