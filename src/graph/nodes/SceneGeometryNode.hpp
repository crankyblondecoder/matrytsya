#ifndef SCENE_GEOMETRY_NODE_H
#define SCENE_GEOMETRY_NODE_H

#include <atomic>

#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/StrobeActionTarget.hpp"
#include "../GraphFocusable.hpp"
#include "../GraphNode.hpp"
#include "SceneGeometry.hpp"

/**
 * Graph node that represents scene geometry, with vertexes populated directly through its C++ API rather
 * than a Lua script.
 */
class SceneGeometryNode : public GraphNode, public SceneActionTarget, public StrobeActionTarget,
	public GraphFocusable, public SceneGeometry
{
    public:

        SceneGeometryNode();

		void populateSurface(GraphHandle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		void setStrobe(bool flag) override;

		SceneActionTarget* getSceneActionTarget() override;

		StrobeActionTarget* getStrobeActionTarget() override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
        virtual ~SceneGeometryNode();

		void _poked(GraphPoke poke) override;

    private:

        // Do not allow copying.
        SceneGeometryNode(const SceneGeometryNode& copyFrom);
        SceneGeometryNode& operator= (const SceneGeometryNode& copyFrom);

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
