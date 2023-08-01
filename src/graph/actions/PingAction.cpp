#include "PingAction.hpp"

PingAction::~PingAction()
{
}

PingAction::PingAction()
{
}

void PingAction::_apply(PingActionTarget* target)
{
	// Just ignore return value for now.
	target -> ping();
}
