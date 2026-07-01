#include "SerialisableAction.hpp"

#include "../GraphNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../actionTargets/SerialisableActionTarget.hpp"

SerialisableAction::~SerialisableAction()
{
}

SerialisableAction::SerialisableAction(GraphNodeHandle& initNode, unsigned energy)
	: GraphAction(initNode, energy)
{
}

unsigned long SerialisableAction::getFlag()
{
	return SERIALISABLE_GRAPH_ACTION;
}

void SerialisableAction::_apply(GraphNode* target)
{
	SerialisableActionTarget* actionTarget = target -> getSerialisableActionTarget();

	if(actionTarget)
	{
		SerialisableActionPayload* payload = _serialise();

		if(payload)
		{
			actionTarget -> send(*payload);
			payload -> decrRef();
		}
	}
}

void SerialisableAction::_complete()
{
}

void SerialisableAction::deserialise(SerialisableActionPayload& data)
{
	_deserialise(data);
}

