#include "SerialisableAction.hpp"

#include "../GraphNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../actionTargets/SerialisableActionTarget.hpp"

SerialisableAction::~SerialisableAction()
{
}

SerialisableAction::SerialisableAction(GraphHandle<GraphNode>& initNode, unsigned energy)
	: GraphAction(initNode, energy)
{
	// Many actions that are serialisable will need to still be invoked on non-serialising nodes.
	_addFlag(SERIALISABLE_GRAPH_ACTION, false);
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

