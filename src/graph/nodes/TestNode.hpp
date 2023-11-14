#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../actionTargets/PingActionTarget.hpp"
#include "../GraphNode.hpp"

/** Test graph node. */
class TestNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~TestNode();

        TestNode(Graph* graph);

		virtual bool ping();

		/** Emit a ping action from this node. */
		void emitPing();

	protected:

		void _detached();

		bool _canActionTarget(GraphAction*);

    private:
};

#endif
