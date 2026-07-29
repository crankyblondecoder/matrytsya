#include "../actions/PingAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../../util/Handle.hpp"
#include "PingNode.hpp"

PingNode::~PingNode()
{
}

PingNode::PingNode() : GraphSerialisedActionNode()
{
	_setEnergyCost(1);

	// Supports ping action.
	_addActionFlag(PING_GRAPH_ACTION);
}

GraphNode::Type PingNode::getType()
{
	return Type::PING_NODE;
}

bool PingNode::ping()
{
	_pingCount++;
	return true;
}

PingAction* PingNode::emitPing(bool wait)
{
	Handle<GraphNode> handle(this);

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

void PingNode::_poked(GraphPoke poke)
{
}

