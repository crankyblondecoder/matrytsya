#ifndef SCENE_TRANSFORM_NODE_H
#define SCENE_TRANSFORM_NODE_H

#include "../GraphNode.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "../graphSceneElements.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that represents a transform applied to scene geometry.
 */
class SceneTransformNode : public GraphNode, public SceneActionTarget
{
    public:

        virtual ~SceneTransformNode();

        SceneTransformNode();

		/**
		 * Set the transform applied to this
		 * @param transform The transform to set.
		 * @param isWorld Whether this transform affects the accumulative world transform.
		 */
		void setTransform(Transform transform, bool isWorld);

		void populateSurface(GraphHiveSceneSurface& surface) override;

		SceneActionTarget* getSceneActionTarget() override;

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

		/**
		 * Whether the transform is multipled to the current world transform.
		 * If true this will affect all scene nodes that are processsed after this one as an action traverses the
		 * graph.
		 */
		bool _transformAccumulates = false;
};

#endif
