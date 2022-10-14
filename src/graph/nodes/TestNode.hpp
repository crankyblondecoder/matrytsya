#ifndef TEST_NODE_H
#define TEST_NODE_H

#include "../actionTargets/TestActionTarget.hpp"
#include "../GraphNode.hpp"

/** Test graph node. */
class TestNode : public GraphNode, public TestActionTarget
{
    public:

        virtual ~TestNode();

        TestNode();

		virtual bool testPing();

    private:
};

#endif
