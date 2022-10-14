#include "TestAction.hpp"

TestAction::~TestAction()
{
}

TestAction::TestAction()
{
}

void TestAction::apply(TestActionTarget* target)
{
	// Apply action to target interface.

	// Ping test. Just ignore return value for now.
	target -> testPing();
}
