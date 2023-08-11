#include "TestNode.hpp"

TestNode::~TestNode()
{

}

TestNode::TestNode(Graph* graph) : GraphNode(graph)
{
}

bool TestNode::testPing()
{
	return true;
}
