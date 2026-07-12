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

		struct Scene
		{
			/// Chunks that make up a scene.
			std::vector<Chunk> chunks;

			/// The model transforms that apply to scene chunks.
			std::vector<ModelTransform> modelTransforms;
		};

		virtual void activate() override;

		/**
		 * Add vertexes to this scene surface to create a new chunk.
		 * Think of this as adding vertexes to a stream.
		 * @note The model transform that is ultimately applied to this chunk is the current model transform.
		 * @note This function is not thread safe.
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
		 * @note This function is not thread safe.
		 * @param transform The local transform to add.
		 * @param id Identifying value stored against the new model transform. This is not required to be unique but
		 *        any transforms with the same id will be considered to be the effectively the same.
		 */
		void addLocalTransform(const Transform& transform, unsigned id);

		/**
		 * Get a copy of the surfaces current scene.
		 */
		Scene getScene();

	protected:

		// Required by ref counting.
		virtual ~GraphHiveSceneSurface();

		virtual void _populateStart() override;

		virtual void _populateEnd() override;

		virtual void _close() override;

	private:

		// Disable copying.
		GraphHiveSceneSurface(const GraphHiveSceneSurface& copyFrom);
		GraphHiveSceneSurface& operator= (const GraphHiveSceneSurface& copyFrom);

		/// Chunks that describe the surface currently being built.
		std::vector<Chunk> _chunks;

		/// The model transforms that apply to the surface chunks currently being built. The last transform in this list is the "current" one.
		std::vector<ModelTransform> _modelTransforms;

		/// The currently scene that the surface can display.
		Scene _currentScene;

		/// Generic lock.
		ThreadMutex _lock;

		/// The scene root node this scene surface is bound to.
		GraphHandle<SceneRootNode> _boundRootNode;
};

#endif
