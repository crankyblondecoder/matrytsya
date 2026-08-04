#include "TriggerAction.hpp"

#include <cstdint>

#include "../actionTargets/TriggerActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"
#include "SerialisableActionPayload.hpp"

TriggerAction::~TriggerAction()
{
}

TriggerAction::TriggerAction(Handle<GraphNode>& initNode, std::string nodeName, bool restrictToNodeType,
	GraphNodeType nodeType)
	: SerialisableAction(initNode, _startingEnergy), _nodeName(nodeName), _restrictToNodeType(restrictToNodeType),
	  _nodeType(nodeType)
{
	_addFlag(TRIGGER_GRAPH_ACTION, false);
}

bool TriggerAction::_apply(GraphNode* target)
{
	if(!_nodeName.empty() && target -> getName() != _nodeName) return false;
	if(_restrictToNodeType && target -> getType() != _nodeType) return false;

	TriggerActionTarget* triggerTarget = target -> getTriggerActionTarget();

	if(triggerTarget)
	{
		triggerTarget -> trigger();
	}

	// Any serialisation should happen after all other actions are applied.
	return SerialisableAction::_apply(target);
}

bool TriggerAction::_starting()
{
	return true;
}

void TriggerAction::_complete()
{
}

SerialisableActionPayload* TriggerAction::_serialise()
{
	uint32_t nodeNameLength = static_cast<uint32_t>(_nodeName.size());

	unsigned sizeInBytes = nodeNameLength + sizeof(nodeNameLength) + sizeof(uint8_t) + sizeof(uint32_t);

	SerialisableActionPayload* payload = new SerialisableActionPayload(SerialisableActionType::TRIGGER, sizeInBytes);

	payload -> serialiseValue(_nodeName.data(), nodeNameLength);
	payload -> serialiseValue(nodeNameLength);
	payload -> serialiseValue(static_cast<uint8_t>(_restrictToNodeType));
	payload -> serialiseValue(static_cast<uint32_t>(_nodeType));

	return payload;
}

void TriggerAction::_deserialise(SerialisableActionPayload& data)
{
	uint32_t nodeType;
	data.deserialiseValue(nodeType);
	_nodeType = static_cast<GraphNodeType>(nodeType);

	uint8_t restrictToNodeType;
	data.deserialiseValue(restrictToNodeType);
	_restrictToNodeType = restrictToNodeType;

	uint32_t nodeNameLength;
	data.deserialiseValue(nodeNameLength);

	_nodeName.resize(nodeNameLength);
	data.deserialiseValue(_nodeName.data(), nodeNameLength);
}

SerialisableAction::SerialisableActionType TriggerAction::getSerialisbleType()
{
	return SerialisableActionType::TRIGGER;
}
