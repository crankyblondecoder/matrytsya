#ifndef PING_TEST_H
#define PING_TEST_H

#include <gtest/gtest.h>

#include "../../graph/actions/PingAction.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphHiveCollection.hpp"
#include "../../graph/GraphHiveHandle.hpp"
#include "../../graph/graphEdgeFlagRegister.hpp"
#include "../../graph/GraphNodeHandle.hpp"
#include "../../graph/GraphNodeLocation.hpp"
#include "../../graph/nodes/PingNode.hpp"
#include "../../graph/nodes/TeleportNode.hpp"

TEST(PingTest, ActionEnergyRundown)
{
	// This creates a hive and runs an action that should cycle until it runs out of energy.

	GraphHive* hive = new GraphHive(2);

	GraphHiveHandle hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	PingNode* testNode1 = new PingNode();
	PingNode* testNode2 = new PingNode();
	PingNode* testNode3 = new PingNode();

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
	// This gives a combined ping count of 32 across all nodes.
	unsigned pingCount = testNode1 -> getPingCount() + testNode2 -> getPingCount() + testNode3 -> getPingCount();
	EXPECT_EQ(pingCount, 32u) << "Ping count incorrect.";

	action -> decrRef();

	hive -> shutdown();
}

TEST(PingTest, ActionEnergyRundownWaitOnHive)
{
	// This is the same test as ActionEnergyRundown, but waits on the hive's action
	// activity conditions rather than waiting on the action itself to complete.

	GraphHive* hive = new GraphHive(2);

	GraphHiveHandle hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	PingNode* testNode1 = new PingNode();
	PingNode* testNode2 = new PingNode();
	PingNode* testNode3 = new PingNode();

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

	// Emit ping action without waiting on it directly.
	PingAction* action = testNode1 -> emitPing(false);

	// Wait for the action to become active, then wait for it to run to completion.
	hive -> waitOnInitialActionActive(0);
	hive -> waitOnNoActionsActive(0);

	// Ping action should have run out of energy.
	EXPECT_EQ(action -> getEnergyLevel(), 0) << "Energy was not zero as expected.";

	// Assume standard energy for ping action is 32 and test node consumes 1 unit per traversal.
	// This gives a combined ping count of 32 across all nodes.
	unsigned pingCount = testNode1 -> getPingCount() + testNode2 -> getPingCount() + testNode3 -> getPingCount();
	EXPECT_EQ(pingCount, 32u) << "Ping count incorrect.";

	action -> decrRef();

	hive -> shutdown();
}

TEST(PingTest, ActionTeleportBetweenHives)
{
	// Two hives in a single collection. Hive1 is a 5 node cycle of 4 ping nodes followed by a
	// teleport node; the teleport node points at the first ping node of hive2's own 5 node ping
	// cycle. Each time the hive1 action passes through the teleport node it spawns a fresh ping
	// action (full energy) in hive2.

	GraphHive* hive1 = new GraphHive(2);
	GraphHive* hive2 = new GraphHive(2);

	hive1 -> setName("hive1");
	hive2 -> setName("hive2");

	GraphHiveHandle hiveHandle1(hive1);
	GraphHiveHandle hiveHandle2(hive2);

	GraphHiveCollection collection;

	collection.addHive(hiveHandle1);
	collection.addHive(hiveHandle2);

	hive1 -> setHiveCollection(&collection);
	hive2 -> setHiveCollection(&collection);

	// -- Build hive2's 5 node ping cycle --

	PingNode* h2Node1 = new PingNode();
	PingNode* h2Node2 = new PingNode();
	PingNode* h2Node3 = new PingNode();
	PingNode* h2Node4 = new PingNode();
	PingNode* h2Node5 = new PingNode();

	h2Node1 -> setName("h2Node1");

	hive2 -> addNode(h2Node1);
	hive2 -> addNode(h2Node2);
	hive2 -> addNode(h2Node3);
	hive2 -> addNode(h2Node4);
	hive2 -> addNode(h2Node5);

	GraphNodeHandle h2Handle1(h2Node1);
	GraphNodeHandle h2Handle2(h2Node2);
	GraphNodeHandle h2Handle3(h2Node3);
	GraphNodeHandle h2Handle4(h2Node4);
	GraphNodeHandle h2Handle5(h2Node5);

	h2Node1 -> createEdge(h2Handle2);
	h2Node2 -> createEdge(h2Handle3);
	h2Node3 -> createEdge(h2Handle4);
	h2Node4 -> createEdge(h2Handle5);
	h2Node5 -> createEdge(h2Handle1);

	// -- Build hive1's 5 node cycle: 4 ping nodes then a teleport node back to node 1 --

	GraphNodeLocation destination;
	destination.setHiveName("hive2");
	destination.setNodeName("h2Node1");

	PingNode* h1Node1 = new PingNode();
	PingNode* h1Node2 = new PingNode();
	PingNode* h1Node3 = new PingNode();
	PingNode* h1Node4 = new PingNode();
	TeleportNode* h1Node5 = new TeleportNode(destination);

	hive1 -> addNode(h1Node1);
	hive1 -> addNode(h1Node2);
	hive1 -> addNode(h1Node3);
	hive1 -> addNode(h1Node4);
	hive1 -> addNode(h1Node5);

	GraphNodeHandle h1Handle1(h1Node1);
	GraphNodeHandle h1Handle2(h1Node2);
	GraphNodeHandle h1Handle3(h1Node3);
	GraphNodeHandle h1Handle4(h1Node4);
	GraphNodeHandle h1Handle5(h1Node5);

	h1Node1 -> createEdge(h1Handle2);
	h1Node2 -> createEdge(h1Handle3);
	h1Node3 -> createEdge(h1Handle4);
	h1Node4 -> createEdge(h1Handle5);
	h1Node5 -> createEdge(h1Handle1);

	// Run the ping action within hive1. Waiting on it directly only guarantees hive1's own
	// traversal has finished; the teleported actions it spawned in hive2 keep running.
	PingAction* action = h1Node1 -> emitPing(true);

	hive2 -> waitOnInitialActionActive(5000);

	// There should be a total of 6 teleported actions.
	hive2 -> waitOnActionActiveCountAccum(6, 5000);

	hive2 -> waitOnNoActionsActive(5000);

	// Ping action should have run out of energy.
	EXPECT_EQ(action -> getEnergyLevel(), 0) << "Energy was not zero as expected.";

	// Of the 32 units of energy, one is spent on the teleport node every 5th step (steps 4, 9,
	// 14, 19, 24 and 29), leaving 26 spent on hive1's 4 ping nodes and 6 teleports triggered.
	unsigned hive1PingCount = h1Node1 -> getPingCount() + h1Node2 -> getPingCount() +
		h1Node3 -> getPingCount() + h1Node4 -> getPingCount();

	EXPECT_EQ(hive1PingCount, 26u) << "Hive1 ping count incorrect.";

	// Each of the 6 teleported actions starts with fresh energy (32) and runs entirely within
	// hive2's all ping node cycle, so each one contributes a combined ping count of 32.
	unsigned hive2PingCount = h2Node1 -> getPingCount() + h2Node2 -> getPingCount() +
		h2Node3 -> getPingCount() + h2Node4 -> getPingCount() + h2Node5 -> getPingCount();

	EXPECT_EQ(hive2PingCount, 192u) << "Hive2 ping count incorrect.";

	action -> decrRef();

	collection.shutdown();
}

#endif
