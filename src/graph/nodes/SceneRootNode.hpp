#ifndef SCENE_ROOT_NODE_H
#define SCENE_ROOT_NODE_H

#include "../GraphNode.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that roots a visual scene.
 */
class SceneRootNode : public GraphNode
{
    public:

        virtual ~SceneRootNode();

        SceneRootNode();

		/**
		 * Populate the given scene surface.
		 */
		void populateSceneSurface(GraphHandle<GraphHiveSceneSurface> sceneSurface);

		/**
		 * Emit a single strobe action from this node immediately.
		 */
		void emitStrobe();

	protected:

	private:

        // Do not allow copying.
        SceneRootNode(const SceneRootNode& copyFrom);
        SceneRootNode& operator= (const SceneRootNode& copyFrom);
};

#endif
