#include "TestNode.hpp"

TestNode::~TestNode()
{

}

TestNode::TestNode()
{
	// Set cost in energy units to run the Test Action on this node.
	// This overrides the default cost in TestActionTarget.
	TestActionTarget::setEnergyCost(2);
}

bool TestNode::testPing()
{
	return true;
}
