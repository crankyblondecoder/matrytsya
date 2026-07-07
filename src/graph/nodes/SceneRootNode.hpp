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
		 * Generate a scene surface for the visual scene rooted at this node.
		 * @note Emits a SceneAction from this node and waits for it to complete before returning.
		 * @param timeOut Maximum period in ms to wait for the action to complete. Use 0 to wait indefinitely.
		 * @returns Newly created surface, populated by traversing the scene rooted at this node. Not owned by this;
		 *          caller is responsible for deleting it.
		 * @throw GraphException SCENE_SURFACE_GENERATION_TIMED_OUT if the action doesn't complete within timeOut.
		 */
		GraphHiveSceneSurface* generateSceneSurface(unsigned timeOut);

	protected:

	private:

        // Do not allow copying.
        SceneRootNode(const SceneRootNode& copyFrom);
        SceneRootNode& operator= (const SceneRootNode& copyFrom);
};

#endif
