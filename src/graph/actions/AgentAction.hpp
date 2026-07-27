#ifndef AGENT_ACTION_H
#define AGENT_ACTION_H

#include <string>
#include <vector>

#include "../../agent/AgenticHarness.hpp"
#include "../GraphAction.hpp"
#include "../GraphNode.hpp"
#include "../../util/Handle.hpp"

class ModelContext;

/**
 * Graph action that drives a per-node agentic conversation as it traverses the graph, carrying the same
 * AgenticHarness::Role::NODE conversation forward from one visited node to the next.
 */
class AgentAction : public GraphAction
{
    public:

		/**
		 * Pairs a prompt with the node it applies to.
		 */
		struct NodePrompt
		{
			/// Name of the node this prompt applies to. If empty, no name matching is performed and
			/// only the node type must match.
			std::string nodeIdentifier = "";

			/// Type of node this prompt applies to. Must be satisfied for the prompt to be used.
			GraphNode::Type nodeType = GraphNode::Type::GRAPH_NODE;

			/// Prompt sent as part of this action's agentic request when this entry matches the node
			/// being applied to.
			std::string prompt;
		};

        virtual ~AgentAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param capability Capability required of the model servicing this action's prompts.
		 * @param prompts Prompts to send, each paired with the node it applies to.
		 */
		AgentAction(Handle<GraphNode> initNode, AgenticHarness::Capability capability, std::vector<NodePrompt> prompts);

		/**
		 * Get the model context the matched prompts have been processed within so far.
		 * @returns The context. Invalid handle if no node matching one of the prompts has been visited yet.
		 */
		Handle<ModelContext> getModelContext();

	protected:

		void _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        AgentAction(const AgentAction& copyFrom);
        AgentAction& operator= (const AgentAction& copyFrom);

		/**
		 * Find the prompt entry that matches the given node's name.
		 * @param target Node to match against.
		 * @returns Pointer to the matching entry. Null if none of the prompts match the node.
		 */
		const NodePrompt* __findPrompt(GraphNode* target);

		/// Capability required of the model servicing this action's prompts.
		AgenticHarness::Capability _capability;

		/// Prompts to send, each paired with the node it applies to.
		std::vector<NodePrompt> _prompts;

		/// Context the matched prompts have been processed within so far. Invalid until the first
		/// matching node is visited.
		Handle<ModelContext> _context;
};

#endif
