#include "graphActionFlagRegister.hpp"
#include "GraphAction.hpp"

#include "GraphActionTargetable.hpp"

GraphActionTargetable::~GraphActionTargetable()
{
}

GraphActionTargetable::GraphActionTargetable()
{
}

void GraphActionTargetable::_addActionFlag(unsigned long actionFlag)
{
	_actionFlags |= actionFlag;
}

bool GraphActionTargetable::canActionTarget(GraphAction* graphAction)
{
	return graphAction -> getFlag() & _actionFlags;
}

bool GraphActionTargetable::hasActionTarget(unsigned long actionFlag)
{
	return actionFlag & _actionFlags;
}

PingActionTarget* GraphActionTargetable::getPingActionTarget()
{
	return 0;
}

SerialisableActionTarget* GraphActionTargetable::getSerialisableActionTarget()
{
	return 0;
}

