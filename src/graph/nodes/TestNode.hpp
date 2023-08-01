#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../actionTargets/PingActionTarget.hpp"
#include "../GraphNode.hpp"

/** Test graph node. */
class TestNode : public GraphNode, public PingActionTarget
{
    public:

        virtual ~TestNode();

        TestNode();

		virtual bool testPing();

    private:
};

#endif
