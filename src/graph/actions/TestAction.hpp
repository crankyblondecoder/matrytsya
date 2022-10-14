#ifndef TEST_ACTION_H
#define TEST_ACTION_H

#include "../GraphActionTargetBinding.hpp"
#include "../actionTargets/TestActionTarget.hpp"

/**
 * Test graph action.
 */
class TestAction : public GraphActionTargetBinding<TestActionTarget>
{
    public:

        virtual ~TestAction();

		TestAction();

		virtual void apply(TestActionTarget*);

    private:
};

#endif
