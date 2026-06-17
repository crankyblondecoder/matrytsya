#ifndef PING_TEST_H
#define PING_TEST_H

#include <gtest/gtest.h>

#include "../../graph/actions/PingAction.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/graphEdgeFlagRegister.hpp"
#include "../../graph/GraphNodeHandle.hpp"
#include "../../graph/nodes/TestNode.hpp"

TEST(PingTest, ActionEnergyRundown)
{
	// This creates a hive and runs an action that should cycle until it runs out of energy.

	GraphHive* hive = new GraphHive(2);

	GraphHiveHandle hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	TestNode* testNode1 = new TestNode();
	TestNode* testNode2 = new TestNode();
	TestNode* testNode3 = new TestNode();

	hive -> addNode(testNode1);
	hive -> addNode(testNode2);
	hive -> addNode(testNode3);

	GraphNodeHandle nodeHandle1(testNode1);
	GraphNodeHandle nodeHandle2(testNode2);
	GraphNodeHandle nodeHandle3(testNode3);

	// Deliberately create a cycle.
	testNode1 -> createEdge(nodeHandle2);
	testNode2 -> createEdge(nodeHandle3);
	testNode3 -> createEdge(nodeHandle1);

	// Run ping action.
	PingAction* action = testNode1 -> emitPing(true);

	// Ping action should have run out of energy.
	EXPECT_EQ(action -> getEnergyLevel(), 0) << "Energy was not zero as expected.";

	// Assume standard energy for ping action is 32 and test node consumes 1 unit per traversal.
	// This gives a ping count of 32.
	unsigned pingCount = action -> getPingCount();
	EXPECT_EQ(pingCount, 32u) << "Ping count incorrect.";

	action -> decrRef();

	hive -> shutdown();
}

#endif
