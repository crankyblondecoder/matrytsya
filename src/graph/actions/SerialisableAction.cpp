#include "../graphActionFlagRegister.hpp"
#include "SerialisableAction.hpp"

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

void SerialisableAction::apply(SerialisableActionTarget* target)
{
}

void SerialisableAction::_complete()
{
}

