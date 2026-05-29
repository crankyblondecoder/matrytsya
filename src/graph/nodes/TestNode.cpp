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

void TestNode::emitPing()
{
	GraphNodeHandle handle(this);

	// Action will self delete once complete.
	_emitAction(new PingAction(handle));
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

