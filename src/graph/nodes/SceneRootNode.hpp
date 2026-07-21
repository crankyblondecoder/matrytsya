#ifndef SCENE_ROOT_NODE_H
#define SCENE_ROOT_NODE_H

#include <atomic>

#include "StrobeEmitterNode.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that roots a visual scene.
 */
class SceneRootNode : public StrobeEmitterNode
{
    public:

        SceneRootNode();

		/**
		 * Populate the given scene surface.
		 */
		void populateSceneSurface(GraphHandle<GraphHiveSceneSurface> sceneSurface);

		void notify(NotifyType type) override;

		/**
		 * Get the current scene version.
		 * @note This is incremented each time a SCENE_DATA_CHANGED notification is received, so that surfaces bound
		 *       to this root can tell whether the scene needs to be repopulated.
		 */
		unsigned getSceneVersion();

	protected:

		// Ref counted.
        virtual ~SceneRootNode();

		void _poked(GraphPoke poke) override;

	private:

        // Do not allow copying.
        SceneRootNode(const SceneRootNode& copyFrom);
        SceneRootNode& operator= (const SceneRootNode& copyFrom);

		/// Counter of the scene's version, starting at 1 and incremented on every SCENE_DATA_CHANGED notification.
		std::atomic<unsigned> _sceneVersion{1};
};

#endif
