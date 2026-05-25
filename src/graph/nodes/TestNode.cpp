#include "../actions/PingAction.hpp"

#include "TestNode.hpp"

TestNode::~TestNode()
{
}

TestNode::TestNode(Graph* graph) : GraphNode(graph)
{
}

bool TestNode::ping()
{
	return true;
}

void TestNode::emitPing()
{
	// Action will self delete once complete.
	_emitAction(new PingAction());
}

void TestNode::_decoupled()
{
}

void TestNode::_init()
{
	// Do all action target initialisation here.
	PingActionTarget::_init();
}

