#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../actionTargets/PingActionTarget.hpp"
#include "../GraphNode.hpp"

class PingAction;

/** Test graph node. */
class TestNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~TestNode();

        TestNode(GraphHive& hive);

		virtual bool ping();

		/**
		 * Emit a ping action from this node.
		 * @param wait Wait for action to complete.
		 * @returns Ping action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		PingAction* emitPing(bool wait);

	protected:

		void _init() override;

		void _applyAction(PingAction* action) override;

    private:
};

#endif
