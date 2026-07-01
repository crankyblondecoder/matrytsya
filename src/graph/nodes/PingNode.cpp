#include "../actions/PingAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNodeHandle.hpp"
#include "PingNode.hpp"

PingNode::~PingNode()
{
}

PingNode::PingNode() : GraphNode()
{
	_setEnergyCost(1);

	// Supports ping action.
	_addActionFlag(PING_GRAPH_ACTION);
}

bool PingNode::ping()
{
	_pingCount++;
	return true;
}

PingAction* PingNode::emitPing(bool wait)
{
	GraphNodeHandle handle(this);

	// Action will self delete once complete.
	PingAction* action = new PingAction(handle);

	action -> incrRef();

	_emitAction(action);

	if(wait) action -> waitOnComplete(0);

	return action;
}

PingActionTarget* PingNode::getPingActionTarget()
{
	return this;
}

unsigned PingNode::getPingCount()
{
	return _pingCount;
}

