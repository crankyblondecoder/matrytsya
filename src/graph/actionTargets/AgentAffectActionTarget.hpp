#ifndef AGENT_AFFECT_ACTION_TARGET_H
#define AGENT_AFFECT_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that can be marked as having an agentic action applied to it, or something,
 * "close" to it, so that parts of it that are only meant to be seen while that is the case can be revealed.
 */
class AgentAffectActionTarget : virtual public ActionTarget
{
    public:

        virtual ~AgentAffectActionTarget() {}

		AgentAffectActionTarget() {}

		/**
		 * Called when an agentic action starts being applied to the node (direct) or when an agent affecting action
		 * is applied to this target.
		 * @param direct True if an agent action is directly affecting this target.
		 */
		virtual void agentAffectingStart(bool direct) = 0;

		/**
		 * Called when an agentic action end being applied to the node (direct) or when an agent affecting action
		 * is applied to this target.
		 * @param direct True if an agent action is directly affecting this target.
		 */
		virtual void agentAffectingEnd(bool direct) = 0;

	protected:

    private:
};

#endif
