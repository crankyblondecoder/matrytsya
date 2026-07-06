#ifndef SCENE_GEOMETRY_NODE_H
#define SCENE_GEOMETRY_NODE_H

#include <string>
#include <vector>

#include "../graphSceneElements.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "ScriptNode.hpp"

/**
 * Graph node that represents scene geometry.
 */
class SceneGeometryNode : public ScriptNode, public SceneActionTarget
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

		void populateSurface(GraphHiveSceneSurface& surface) override;

		SceneActionTarget* getSceneActionTarget() override;

	protected:

    private:

        // Do not allow copying.
        SceneGeometryNode(const SceneGeometryNode& copyFrom);
        SceneGeometryNode& operator= (const SceneGeometryNode& copyFrom);

		/**
		 * The vertexes that make up the scene object this node defines.
		 * Each triplet defines a triangle with standard counter-clockwise winding order for the front face.
		 * @note There is no indexing at this stage.
		 */
		std::vector<Vertex> _vertexes;
};

#endif
