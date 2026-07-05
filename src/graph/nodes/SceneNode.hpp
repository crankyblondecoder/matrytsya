#ifndef SCENE_NODE_H
#define SCENE_NODE_H

#include <string>
#include <vector>

#include "ScriptNode.hpp"

/**
 * Graph node that represents a scene.
 */
class SceneNode : public ScriptNode
{
    public:

        virtual ~SceneNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 */
        SceneNode(const std::string& script);

		/**
		 * Data structure that describes a single vertex.
		 */
		struct Vertex
		{
			/// Position: X, Y, Z
			double posn[3];
			/// Colour: R, G, B, A
		    double colour[4];
			/// Texture coordinates: U, V
		    double texCoords[2];
			/// Normal (must be normalised): X, Y, Z
		    double normal[3];
		};

		using Transform = double[16];

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
		bool _transformIsWorld = false;
};

#endif
