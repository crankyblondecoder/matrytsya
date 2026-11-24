#ifndef AGENT_AFFECT_ACTION_TARGET_H
#define AGENT_AFFECT_ACTION_TARGET_H

#include <string>

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

		/**
		 * Called each time an agentic action directly applied to the node reports progress on the request it is
		 * making for the node, while that request is still being serviced.
		 * @param text Human readable description of the progress.
		 * @note Only ever called between agentAffectingStart(true) and agentAffectingEnd(true), on the thread
		 *       applying the action. Does nothing unless overridden, as most targets have nowhere to show it.
		 */
		virtual void agentProgress(std::string text) {}

	protected:

    private:
};

#endif
