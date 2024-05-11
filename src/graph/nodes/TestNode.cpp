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

bool TestNode::_canActionTarget(GraphAction* action)
{
	return GraphActionTargetable::_canActionTarget(action);
}