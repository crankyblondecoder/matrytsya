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

void TestNode::_detached()
{
}