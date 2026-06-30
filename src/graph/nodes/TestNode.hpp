#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../GraphNode.hpp"
#include "../actionTargets/PingActionTarget.hpp"

class PingAction;

/** Test graph node. */
class TestNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~TestNode();

        TestNode();

		virtual bool ping() override;

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
