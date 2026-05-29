#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../actionTargets/PingActionTarget.hpp"
#include "../GraphNode.hpp"

/** Test graph node. */
class TestNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~TestNode();

        TestNode(GraphHive& hive);

		virtual bool ping();

		/** Emit a ping action from this node. */
		void emitPing();

	protected:

		void _init() override;

		void _registerActionFlag(unsigned long actionFlag) override;

    private:
};

#endif
