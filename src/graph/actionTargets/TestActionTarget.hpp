#ifndef TEST_ACTION_TARGET_H
#define TEST_ACTION_TARGET_H

#include "../graphActionFlagRegister.hpp"
#include "../GraphActionTarget.hpp"

/**
 * Action target to use for testing purposes only.
 */
class TestActionTarget : public GraphActionTarget<TEST_GRAPH_ACTION>
{
    public:

        virtual ~TestActionTarget();

		TestActionTarget();

		/** Test ping. Should return true in the implementation. */
		virtual bool testPing() = 0;

	protected:

    private:

};

#endif
