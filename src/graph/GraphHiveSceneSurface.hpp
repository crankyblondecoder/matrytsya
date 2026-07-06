#ifndef GRAPH_HIVE_SCENE_SURFACE_H
#define GRAPH_HIVE_SCENE_SURFACE_H

#include <vector>

#include "GraphHiveSurface.hpp"
#include "graphSceneElements.hpp"

/**
 * Surface that enables the contents of a ScriptNode to be serialised by copying it.
 */
class GraphHiveSceneSurface : public GraphHiveSurface
{
	public:

		GraphHiveSceneSurface();

		struct Chunk
		{
			/// Transform that is applied to vertexes after being combined with the current world transform.
			Transform transform;

			/**
			 * Whether this transform accumulates with the world transform, i.e. It is multiplied to it and affects
			 * all subsequent vertexes.
			 */
			bool accumulateTransform;

			/// The vertexes of the chunk. These _must_ be in multiples of three, i.e. three vertexes per triangle.
			std::vector<Vertex> vertexes;
		};

		/**
		 * Add vertexes to this scene surface.
		 * Think of this as adding vertexes to a stream.
		 * @param vertexes Vertexes to add.
		 * @param transform Transform to apply to vertexes.
		 * @param transformAccumulates Whether the transform accumulates, i.e. Is multiplied to the current
		 *        (accumulative) world transform.
		 */
		void addVertexes(const std::vector<Vertex>& vertexes, Transform transform, bool transformAccumulates);

	protected:

		virtual ~GraphHiveSceneSurface();

	private:

		// Disable copying.
		GraphHiveSceneSurface(const GraphHiveSceneSurface& copyFrom);
		GraphHiveSceneSurface& operator= (const GraphHiveSceneSurface& copyFrom);

		/// Chunks that describe the scene surface.
		std::vector<Chunk> _chunks;
};

#endif
