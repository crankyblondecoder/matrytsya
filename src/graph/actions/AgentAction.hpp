#ifndef AGENT_ACTION_H
#define AGENT_ACTION_H

#include <string>
#include <vector>

#include "../../agent/AgenticHarness.hpp"
#include "../GraphAction.hpp"
#include "../GraphNode.hpp"
#include "../../agent/ModelRequestProgressListener.hpp"
#include "../../thread/ThreadMutex.hpp"
#include "../../util/Handle.hpp"

class AgentAffectActionTarget;
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
			GraphNodeType nodeType = GraphNodeType::GRAPH_NODE;

			/// Prompt sent as part of this action's agentic request when this entry matches the node
			/// being applied to.
			std::string prompt;

			/// Terminate (complete) the action once the prompt has completed processing.
			bool terminateOnResponse = false;
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

		/**
		 * Get the progress reported by the agentic requests this action has made, while they were being
		 * serviced.
		 * @returns Every progress report received over the life of this action, oldest first, across every node
		 *          visited.
		 */
		std::vector<std::string> getProgress();

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        AgentAction(const AgentAction& copyFrom);
        AgentAction& operator= (const AgentAction& copyFrom);

		/**
		 * Relays the progress of one agentic request made by an action to that action, along with the node it was
		 * made for.
		 */
		class ProgressRelay : public ModelRequestProgressListener
		{
			public:

				/**
				 * @param action Action that made the request.
				 * @param target Target of the node the request was made for. Null when the node has none.
				 */
				ProgressRelay(AgentAction& action, AgentAffectActionTarget* target);

				virtual void agentRequestProgressReport(std::string text) override;

			private:

				// Disable copying.
				ProgressRelay(const ProgressRelay& copyFrom);
				ProgressRelay& operator= (const ProgressRelay& copyFrom);

				/// Action that made the request.
				AgentAction& _action;

				/// Target of the node the request was made for. Null when the node has none.
				AgentAffectActionTarget* _target;
		};

		/**
		 * Record a progress report and pass it on to the node the request was made for.
		 * @param text Human readable description of the progress.
		 * @param target Target of the node the request was made for. Null when the node has none.
		 */
		void __receiveProgress(std::string text, AgentAffectActionTarget* target);

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

		/// Progress reported by the agentic requests this action has made, oldest first.
		std::vector<std::string> _progress;

		/// Guards _progress, which is written on the thread applying this action and read from any.
		ThreadMutex _progressLock;
};

#endif
