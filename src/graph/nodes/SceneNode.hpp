#ifndef SCENE_NODE_H
#define SCENE_NODE_H

#include <string>
#include <vector>

#include "../graphSceneElements.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "ScriptNode.hpp"

/**
 * Graph node that represents a scene.
 */
class SceneNode : public ScriptNode, public SceneActionTarget
{
    public:

        virtual ~SceneNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 */
        SceneNode(const std::string& script);

		/**
		 * Add vertexes to the list of vertexes for this scene.
		 */
		void addVertexes(std::vector<Vertex> vertexesToAdd);

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
        SceneNode(const SceneNode& copyFrom);
        SceneNode& operator= (const SceneNode& copyFrom);

		/**
		 * The vertexes that make up the scene object this node defines.
		 * Each triplet defines a triangle with standard counter-clockwise winding order for the front face.
		 * @note There is no indexing at this stage.
		 */
		std::vector<Vertex> _vertexes;

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
