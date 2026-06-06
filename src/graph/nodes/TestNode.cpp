#include "../actions/PingAction.hpp"

#include "../GraphNodeHandle.hpp"
#include "TestNode.hpp"

TestNode::~TestNode()
{
}

TestNode::TestNode(GraphHive& hive) : GraphNode(hive)
{
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
	// Do all action target initialisation here.
	PingActionTarget::_init();
}

void TestNode::_registerActionFlag(unsigned long actionFlag)
{
	_addActionFlag(actionFlag);
}

