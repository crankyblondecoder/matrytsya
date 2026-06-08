#include "../actions/PingAction.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNodeHandle.hpp"
#include "TestNode.hpp"

TestNode::~TestNode()
{
}

TestNode::TestNode(GraphHive& hive) : GraphNode(hive)
{
	_setEnergyCost(1);

	// Supports ping action.
	_addActionFlag(PING_GRAPH_ACTION);
}

bool TestNode::ping()
{
	return true;
}

PingAction* TestNode::emitPing(bool wait)
{
	GraphNodeHandle handle(this);

	// Action will self delete once complete.
	PingAction* action = new PingAction(handle);

	action -> incrRef();

	_emitAction(action);

	action -> waitOnComplete(0);

	return action;
}

void TestNode::_init()
{
}

void TestNode::_applyAction(PingAction* action)
{
	action -> apply(this);
}

