#ifndef AGENT_VISIBLE_ACTION_TARGET_H
#define AGENT_VISIBLE_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that can be marked as having an agentic action applied to it, so that
 * parts of it that are only meant to be seen while that is the case can be revealed.
 * @note This carries no action flag of its own. An action that marks a node this way is already gated on
 *       whatever flag it requires, and a null return from GraphActionTargetable::getAgentVisibleActionTarget()
 *       is what says a node has nothing to reveal.
 */
class AgentVisibleActionTarget : virtual public ActionTarget
{
    public:

        virtual ~AgentVisibleActionTarget() {}

		AgentVisibleActionTarget() {}

		/**
		 * Set whether an agentic action is currently being applied to the node.
		 * @param flag True while the node is being worked on, false once it is not.
		 */
		virtual void setAgentVisible(bool flag) = 0;

		/**
		 * @returns Whether an agentic action is currently being applied to the node.
		 */
		virtual bool getAgentVisible() = 0;

	protected:

    private:
};

#endif
