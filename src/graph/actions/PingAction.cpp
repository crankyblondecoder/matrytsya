#include "../graphActionFlagRegister.hpp"
#include "PingAction.hpp"

#include <iostream>

PingAction::~PingAction()
{
}

PingAction::PingAction(GraphNodeHandle& initNode)
	: GraphAction(initNode, 32)
{
	_pingCount = 0;
}

unsigned long PingAction::getFlag()
{
	return PING_GRAPH_ACTION;
}

void PingAction::apply(PingActionTarget* target)
{
	bool pinged = target -> ping();
	if(pinged) _pingCount++;
}

void PingAction::_complete()
{
//	std::cout << "Ping count: " << _pingCount << "\n";
}

unsigned PingAction::getPingCount()
{
	return _pingCount;
}
