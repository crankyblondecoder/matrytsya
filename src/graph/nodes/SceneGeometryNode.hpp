#ifndef SCENE_GEOMETRY_NODE_H
#define SCENE_GEOMETRY_NODE_H

#include <atomic>

#include "../actionTargets/AgentAffectActionTarget.hpp"
#include "../actionTargets/SceneActionTarget.hpp"
#include "../actionTargets/StrobeActionTarget.hpp"
#include "../GraphFocusable.hpp"
#include "../GraphNode.hpp"
#include "AgentAffectingActionEmitter.hpp"
#include "SceneGeometry.hpp"

/**
 * Graph node that represents scene geometry, with vertexes populated directly through its C++ API rather
 * than a Lua script.
 */
class SceneGeometryNode : public GraphNode, public SceneActionTarget, public StrobeActionTarget,
	public AgentAffectActionTarget, public GraphFocusable, public SceneGeometry, public AgentAffectingActionEmitter
{
    public:

		/**
		 * @param emitAgentAffectAction If true, this node emits an AgentAffectAction from this node if notified
		 *        that it is being affected directly by an agent.
		 */
        SceneGeometryNode(bool emitAgentAffectAction = false);

		GraphNodeType getType() override;

		void populateSurface(Handle<GraphHiveSceneSurface> surface) override;

		void strobe() override;

		void setStrobe(bool flag) override;

		SceneActionTarget* getSceneActionTarget() override;

		StrobeActionTarget* getStrobeActionTarget() override;

		AgentAffectActionTarget* getAgentAffectActionTarget() override;

		// Agent affect target API point.
		void agentAffectingStart(bool direct) override;

		void agentAffectingEnd(bool direct) override;

		unsigned getVersion() override;

	protected:

		// Ref counted.
        virtual ~SceneGeometryNode();

		void _poked(GraphPoke poke) override;

		void _emitAgentAffectAction(bool agentAffecting) override;

    private:

        // Do not allow copying.
        SceneGeometryNode(const SceneGeometryNode& copyFrom);
        SceneGeometryNode& operator= (const SceneGeometryNode& copyFrom);

		/**
		 * Set the agent visible flag.
		 * @param flag Value to set the agent visible flag to.
		 */
		void __setAgentVisible(bool flag);

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
