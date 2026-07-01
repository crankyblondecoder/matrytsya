#ifndef PING_NODE_H
#define PING_NODE_H

#include "../GraphNode.hpp"
#include "../actionTargets/PingActionTarget.hpp"

class PingAction;

/**
 * Simple graph node that just provides a ping point.
 */
class PingNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~PingNode();

        PingNode();

		bool ping() override;

		/**
		 * Get the number of times this node has been pinged.
		 */
		unsigned getPingCount();

		/**
		 * Emit a ping action from this node.
		 * @param wait Wait for action to complete.
		 * @returns Ping action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		PingAction* emitPing(bool wait);

		PingActionTarget* getPingActionTarget() override;

	protected:

    private:

		unsigned _pingCount = 0;
};

#endif
