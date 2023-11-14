#include "PingAction.hpp"
#include "../graphEdgeFlagRegister.hpp"

#include <iostream>

PingAction::~PingAction()
{
}

PingAction::PingAction()
{
	_pingCount = 0;

	_setEdgeTraversalFlags(TEST_GRAPH_EDGE);
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
