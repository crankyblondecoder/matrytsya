#include "PingAction.hpp"
#include "../graphEdgeFlagRegister.hpp"

#include <iostream>

PingAction::~PingAction()
{
}

PingAction::PingAction(GraphNodeHandle& initNode)
	: GraphActionTargetBinding<PingActionTarget>(initNode, 32)
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
//	std::cout << "Ping count: " << _pingCount << "\n";
}

unsigned PingAction::getPingCount()
{
	return _pingCount;
}
