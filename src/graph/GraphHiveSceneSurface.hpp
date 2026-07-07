#ifndef GRAPH_HIVE_SCENE_SURFACE_H
#define GRAPH_HIVE_SCENE_SURFACE_H

#include <vector>

#include "../thread/ThreadMutex.hpp"
#include "GraphHiveSurface.hpp"
#include "graphSceneElements.hpp"

/**
 * Surface that enables the contents of a ScriptNode to be serialised by copying it.
 */
class GraphHiveSceneSurface : public GraphHiveSurface
{
	public:

		GraphHiveSceneSurface();

		/**
		 * Defines a chunk of geometry.
		 */
		struct Chunk
		{
			/// The vertexes of the chunk. These _must_ be in multiples of three, i.e. three vertexes per triangle.
			std::vector<Vertex> vertexes;

			/// The index of the model transform to use for this chunk.
			unsigned modelTransformIndex;
		};

		struct ModelTransform
		{
			/// A numerical id that can be used to refer to this model transform.
			unsigned id;

			/// Actual model transform.
			Transform transform;
		};

		/**
		 * Add vertexes to this scene surface to create a new chunk.
		 * Think of this as adding vertexes to a stream.
		 * @note The model transform that is ultimately applied to this chunk is the current model transform.
		 * @param vertexes Vertexes to add.
		 */
		void addVertexes(const std::vector<Vertex>& vertexes);

		/**
		 * Add a local transform to the scene.
		 * If there is an existing transform with the given id, it will be copied to the end of the model transform
		 * list. Otherwise the given transform is pre-multiplied to the last stored model transform to make the new
		 * model transform which is added to the end of the model transform list.
		 * @param transform The local transform to add.
		 * @param id Identifying value stored against the new model transform. This is not required to be unique.
		 */
		void addLocalTransform(const Transform& transform, unsigned id);

		/**
		 * Get a copy of the chunks that currently describe this scene surface.
		 * @note Returns a copy, taken under lock, so that the caller does not have to hold this surface's internal
		 *       lock while reading the result.
		 */
		std::vector<Chunk> getChunks();

		/**
		 * Get a copy of the model transforms that currently apply to this scene surface's chunks.
		 * @note Returns a copy, taken under lock, so that the caller does not have to hold this surface's internal
		 *       lock while reading the result.
		 */
		std::vector<ModelTransform> getModelTransforms();

		// Not ref counted, unlike most graph objects: instances are owned directly (e.g. by a GraphHiveSurfaceMap),
		// not via a handle, so the destructor must stay accessible to whatever holds one of these by value.
		virtual ~GraphHiveSceneSurface();

	protected:

	private:

		// Disable copying.
		GraphHiveSceneSurface(const GraphHiveSceneSurface& copyFrom);
		GraphHiveSceneSurface& operator= (const GraphHiveSceneSurface& copyFrom);

		/// Chunks that describe the scene surface.
		std::vector<Chunk> _chunks;

		/// The model transforms that apply to chunks. The last transform in this list is the "current" one.
		std::vector<ModelTransform> _modelTransforms;

		/// Guards _chunks and _modelTransforms, which may be written to and read from on different threads.
		ThreadMutex _lock;
};

#endif
