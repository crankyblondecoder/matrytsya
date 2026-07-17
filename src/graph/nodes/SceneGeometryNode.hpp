#ifndef SCENE_GEOMETRY_NODE_H
#define SCENE_GEOMETRY_NODE_H

#include <atomic>
#include <vector>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/StrobeActionTarget.hpp"
#include "../GraphNode.hpp"
#include "../graphSceneElements.hpp"

/**
 * Graph node that represents scene geometry, with vertexes populated directly through its C++ API rather
 * than a Lua script.
 */
class SceneGeometryNode : public GraphNode, public SceneActionTarget, public StrobeActionTarget
{
    public:

        SceneGeometryNode();

		/**
		 * Add vertexes to the list of vertexes for this scene node.
		 */
		void addVertexes(std::vector<Vertex> vertexesToAdd);

		/**
		 * Add vertexes as an array of raw data.
		 * @param rawData Array of raw data that matches the Vertex struct. Multiple vertexes can be defined.
		 * @param length Length of raw data array. Must be in multiples of VERTEX_SERIAL_SIZE. An incomplete vertex
		 *        at the end of the array will simply be discarded rather than throw an exception.
		 */
		void addVertexes(double* rawData, unsigned length);

		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		void setStrobe(bool flag) override;

		SceneActionTarget* getSceneActionTarget() override;

		StrobeActionTarget* getStrobeActionTarget() override;

	protected:

		// Ref counted.
        virtual ~SceneGeometryNode();

		void _poked(GraphPoke poke) override;

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

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
