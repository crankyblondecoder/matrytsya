#ifndef PING_ACTION_TARGET_H
#define PING_ACTION_TARGET_H

#include "ActionTarget.hpp"
/**
 * Action target to use for pinging a node.
 */
class PingActionTarget : virtual public ActionTarget
{
    public:

        virtual ~PingActionTarget() {}

		PingActionTarget() {}

		/** Ping the node. Should return true in the implementation. */
		virtual bool ping() = 0;

	protected:

    private:
};

#endif
