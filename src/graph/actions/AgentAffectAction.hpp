#ifndef AGENT_AFFECT_ACTION_H
#define AGENT_AFFECT_ACTION_H

#include "../GraphAction.hpp"
#include "../../util/Handle.hpp"

class GraphNode;

/**
 * Graph action that communicates whether an agent action is having an indirect affect on a node.
 */
class AgentAffectAction : public GraphAction
{
    public:

        virtual ~AgentAffectAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param agentAffecting True if an agentic action is starting to affect each targeted node, false otherwise.
		 */
		AgentAffectAction(Handle<GraphNode> initNode, bool agentAffecting);

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

	private:

        // Do not allow copying.
        AgentAffectAction(const AgentAffectAction& copyFrom);
        AgentAffectAction& operator= (const AgentAffectAction& copyFrom);

		/// True if an agentic action is starting to affect each targeted node, false otherwise.
		bool _agentAffecting;
};

#endif
