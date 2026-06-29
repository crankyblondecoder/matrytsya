#include "../graphActionFlagRegister.hpp"
#include "SerialisableActionPayload.hpp"
#include "PingAction.hpp"

#include <iostream>

PingAction::~PingAction()
{
}

PingAction::PingAction(GraphNodeHandle& initNode)
	: SerialisableAction(initNode, 32)
{
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

SerialisableActionPayload* PingAction::_serialise()
{
	SerialisableActionPayload* payload = new SerialisableActionPayload(sizeof(_pingCount));

	payload -> serialiseValue(_pingCount);

	return payload;
}

void PingAction::_deserialise(SerialisableActionPayload& data)
{
}

