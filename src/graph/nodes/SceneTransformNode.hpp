#ifndef SCENE_TRANSFORM_NODE_H
#define SCENE_TRANSFORM_NODE_H

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/SceneStrobeActionTarget.hpp"
#include "../graphSceneElements.hpp"
#include "../GraphNode.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that represents a transform applied to scene geometry.
 */
class SceneTransformNode : public GraphNode, public SceneActionTarget, public SceneStrobeActionTarget
{
    public:

        virtual ~SceneTransformNode();

        SceneTransformNode();

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 */
		void setTransform(Transform transform);

		void populateSurface(GraphHiveSceneSurface& surface) override;

		void strobe() override;

		SceneActionTarget* getSceneActionTarget() override;
		SceneStrobeActionTarget* getSceneStrobeActionTarget() override;

	protected:

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
};

#endif
