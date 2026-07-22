#ifndef SCENE_ROOT_NODE_H
#define SCENE_ROOT_NODE_H

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
		void populateSceneSurface(Handle<GraphHiveSceneSurface> sceneSurface);

	protected:

		// Ref counted.
        virtual ~SceneRootNode();

		void _poked(GraphPoke poke) override;

	private:

        // Do not allow copying.
        SceneRootNode(const SceneRootNode& copyFrom);
        SceneRootNode& operator= (const SceneRootNode& copyFrom);
};

#endif
