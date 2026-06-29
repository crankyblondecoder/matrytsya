#include "actions/PingAction.hpp"
#include "graphActionFlagRegister.hpp"
#include "GraphAction.hpp"
#include "GraphException.hpp"

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

void GraphActionTargetable::applyAction(GraphAction* action)
{
	switch(action -> getFlag())
	{
		case PING_GRAPH_ACTION:

			_applyAction(static_cast<PingAction*>(action));
			break;
	}
}

void GraphActionTargetable::_applyAction(PingAction* action)
{
	// If a node indicates it can apply an action but doesn't. This is a critical bug.
	throw GraphException(GraphException::OPERATION_MISSING);
}

