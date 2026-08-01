#ifndef AGENT_NODE_H
#define AGENT_NODE_H

#include <vector>

#include "../GraphSerialisedActionNode.hpp"
#include "../actionTargets/TriggerActionTarget.hpp"
#include "../actions/AgentAction.hpp"

/**
 * Graph node that holds an agentic prompt set and emits it as an AgentAction when triggered.
 * @note The prompts and the capability they require are fixed at construction, so the node carries everything
 *       an emitted action needs and nothing about it can change between one trigger and the next.
 */
class AgentNode : public GraphSerialisedActionNode, public TriggerActionTarget
{
    public:

		/**
		 * @param capability Capability required of the model servicing the emitted action's prompts.
		 * @param prompts Prompts the emitted action sends, each paired with the node it applies to.
		 * @param autoTriggerAgentAction If true, a trigger arriving at this node emits an agent action. If
		 *        false, trigger() does nothing and an agent action is only emitted by an explicit
		 *        emitAgentAction() call.
		 * @param serialiseEmittedActions Forwarded to GraphSerialisedActionNode. See its constructor for
		 *        details. Defaults to true so that a re-trigger queues behind the conversation already in
		 *        flight rather than running a second one alongside it.
		 */
        AgentNode(AgenticHarness::Capability capability, std::vector<AgentAction::NodePrompt> prompts,
			bool autoTriggerAgentAction = true, bool serialiseEmittedActions = true);

		Type getType() override;

		/**
		 * Emit an agent action carrying this node's prompts from this node.
		 * @param wait Wait for the action to complete.
		 * @returns Agent action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		AgentAction* emitAgentAction(bool wait);

		/**
		 * Emit an agent action carrying this node's prompts from this node, without waiting on it.
		 * @note Does nothing unless this node was constructed with autoTriggerAgentAction true.
		 */
		void trigger() override;

		TriggerActionTarget* getTriggerActionTarget() override;

		/**
		 * Get the capability required of the model servicing the emitted action's prompts.
		 */
		AgenticHarness::Capability getCapability();

		/**
		 * Get the prompts the emitted action sends.
		 */
		const std::vector<AgentAction::NodePrompt>& getPrompts();

		/**
		 * Get whether a trigger arriving at this node emits an agent action.
		 */
		bool getAutoTriggerAgentAction();

	protected:

		// Ref counted.
        virtual ~AgentNode();

		void _poked(GraphPoke poke) override;

    private:

        // Do not allow copying.
        AgentNode(const AgentNode& copyFrom);
        AgentNode& operator= (const AgentNode& copyFrom);

		/// Capability required of the model servicing the emitted action's prompts.
		AgenticHarness::Capability _capability;

		/// Prompts the emitted action sends, each paired with the node it applies to.
		std::vector<AgentAction::NodePrompt> _prompts;

		/// Whether a trigger arriving at this node emits an agent action.
		bool _autoTriggerAgentAction;
};

#endif
