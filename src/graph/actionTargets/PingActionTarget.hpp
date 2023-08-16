#ifndef PING_ACTION_TARGET_H
#define PING_ACTION_TARGET_H

#include "../graphActionFlagRegister.hpp"
#include "../GraphActionTarget.hpp"

/**
 * Action target to use for pinging a node.
 */
class PingActionTarget : public GraphActionTarget<PING_GRAPH_ACTION>
{
    public:

        virtual ~PingActionTarget();

		PingActionTarget();

		/** Ping the node. Should return true in the implementation. */
		virtual bool ping() = 0;

	protected:

		void _complete();

    private:
};

#endif
