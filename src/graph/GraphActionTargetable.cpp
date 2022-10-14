#include "GraphActionTargetable.hpp"

GraphActionTargetable::~GraphActionTargetable()
{
}

GraphActionTargetable::GraphActionTargetable()
{
	_actionFlags = 0;
}

void GraphActionTargetable::addActionFlag(unsigned long actionFlag)
{
	_actionFlags |= actionFlag;
}

bool GraphActionTargetable::canActionTarget(GraphAction* graphAction)
{
	return graphAction -> getFlag() & _actionFlags;
}