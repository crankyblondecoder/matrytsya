#include "PingAction.hpp"

#include <iostream>

PingAction::~PingAction()
{
}

PingAction::PingAction()
{
	_pingCount = 0;
}

void PingAction::_apply(PingActionTarget* target)
{
	// Just ignore return value for now.
	if(target -> ping()) _pingCount++;
}

void PingAction::_complete()
{
	std::cout << "Ping count: " << _pingCount << "\n";
}