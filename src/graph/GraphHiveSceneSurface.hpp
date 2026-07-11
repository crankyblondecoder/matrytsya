#ifndef GRAPH_HIVE_SCENE_SURFACE_H
#define GRAPH_HIVE_SCENE_SURFACE_H

#include <vector>

#include "../thread/ThreadMutex.hpp"
#include "GraphHandle.hpp"
#include "GraphHiveSurface.hpp"
#include "graphSceneElements.hpp"
#include "nodes/SceneRootNode.hpp"

/**
 * Surface that enables the contents of a visual scene pathway to be viewed and interacted with.
 */
class GraphHiveSceneSurface : public GraphHiveSurface
{
	public:

		/**
		 * Create a new scene surface.
		 * @param sceneRootNode The scenes root node.
		 */
		GraphHiveSceneSurface(GraphHandle<SceneRootNode> sceneRootNode);

		/**
		 * Defines a chunk of geometry.
		 */
		struct Chunk
		{
			/// Id that is unique within a scene surface.
			unsigned id;

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

		virtual void activate() override;

		virtual void populateStart() override;

		virtual void populateEnd() override;

		/**
		 * Add vertexes to this scene surface to create a new chunk.
		 * Think of this as adding vertexes to a stream.
		 * @note The model transform that is ultimately applied to this chunk is the current model transform.
		 * @param vertexes Vertexes to add or update (depending on the id).
		 * @param id Id unique for this scene surface. If it matches and existing chunk, that chunk will be updated.
		 */
		void addVertexes(const std::vector<Vertex>& vertexes, unsigned id);

		/**
		 * Add a local transform to the scene.
		 * If there is an existing transform with the given id, it will be copied to the end of the model transform
		 * list, and the given transform ignored. Otherwise the given transform is pre-multiplied to the last
		 * stored model transform to make the new model transform which is added to the end of the model transform
		 * list.
		 * @param transform The local transform to add.
		 * @param id Identifying value stored against the new model transform. This is not required to be unique but
		 *        any transforms with the same id will be considered to be the effectively the same.
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


	protected:

		// Required by ref counting.
		virtual ~GraphHiveSceneSurface();

		virtual void _close() override;

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

		/// The scene root node this scene surface is bound to.
		GraphHandle<SceneRootNode> _boundRootNode;
};

#endif
