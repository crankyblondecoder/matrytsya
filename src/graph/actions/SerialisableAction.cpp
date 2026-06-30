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
		// TODO ...
	}
}

void SerialisableAction::_complete()
{
}

