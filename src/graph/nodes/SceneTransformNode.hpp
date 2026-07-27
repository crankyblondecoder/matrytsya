#ifndef SCENE_TRANSFORM_NODE_H
#define SCENE_TRANSFORM_NODE_H

#include <atomic>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/StrobeActionTarget.hpp"
#include "../GraphNode.hpp"
#include "SceneTransform.hpp"

class GraphHiveSceneSurface;

/**
 * Graph node that represents a transform applied to scene geometry, set directly through its C++ API
 * rather than a Lua script.
 */
class SceneTransformNode : public GraphNode, public SceneActionTarget, public StrobeActionTarget, public SceneTransform
{
    public:

        SceneTransformNode();

		Type getType() override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		void setStrobe(bool flag) override;

		SceneActionTarget* getSceneActionTarget() override;

		StrobeActionTarget* getStrobeActionTarget() override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
        virtual ~SceneTransformNode();

		void _poked(GraphPoke poke) override;

    private:

        // Do not allow copying.
        SceneTransformNode(const SceneTransformNode& copyFrom);
        SceneTransformNode& operator= (const SceneTransformNode& copyFrom);

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
