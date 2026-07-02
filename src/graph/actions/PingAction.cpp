#include "PingAction.hpp"

#include <cstdint>

#include "../actionTargets/PingActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"
#include "SerialisableActionPayload.hpp"

PingAction::~PingAction()
{
}

PingAction::PingAction(GraphNodeHandle& initNode)
	: SerialisableAction(initNode, 32)
{
	_addFlag(PING_GRAPH_ACTION);
}

void PingAction::_apply(GraphNode* target)
{
	PingActionTarget* pingTarget = target -> getPingActionTarget();

	if(pingTarget)
	{
		bool pinged = pingTarget -> ping();
		if(pinged) _pingCount++;
	}

	// Any serialisation should happen after all other actions are applied.
	SerialisableAction::_apply(target);
}

void PingAction::_complete()
{
}

unsigned PingAction::getPingCount()
{
	return _pingCount;
}

SerialisableActionPayload* PingAction::_serialise()
{
	SerialisableActionPayload* payload = new SerialisableActionPayload(SerialisableActionType::PING, sizeof(_pingCount));

	payload -> serialiseValue(_pingCount);

	return payload;
}

void PingAction::_deserialise(SerialisableActionPayload& data)
{
	uint32_t pingCount;
	data.deserialiseValue(pingCount);
	_pingCount = pingCount;
}

SerialisableAction::SerialisableActionType PingAction::getSerialisbleType()
{
	return SerialisableActionType::PING;
}

