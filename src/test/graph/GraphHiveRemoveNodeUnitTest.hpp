#ifndef GRAPH_HIVE_REMOVE_NODE_UNIT_TEST_H
#define GRAPH_HIVE_REMOVE_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/GraphEdge.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/graphActionFlagRegister.hpp"
#include "../../graph/nodes/PingNode.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/json/JsonHiveLoader.hpp"
#include "../../util/Handle.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
	/// A ring a -> b -> a, a second way into b from c, a self loop on b, and a strobed scene root.
	const char* const _REMOVE_NODE_TEST_HIVE_JSON = R"({
		"name": "RemoveNodeHive",
		"nodes": [
			{ "type": "PingNode", "name": "a", "edges": [ { "toNodeName": "b", "actionFlags": ["PING_GRAPH_ACTION"], "traversalKey": 4 } ] },
			{ "type": "PingNode", "name": "b", "edges": [ { "toNodeName": "a" }, { "toNodeName": "b", "actionsCompleteAfterTraverse": true } ] },
			{ "type": "PingNode", "name": "c", "edges": [ { "toNodeName": "b" } ] },
			{ "type": "SceneRootNode", "name": "root", "edges": [ { "toNodeName": "a" } ] }
		],
		"strobeEmitters": [ { "nodeName": "root", "periodMs": 1000 } ]
	})";

	GraphHive* _buildRemoveNodeTestHive()
	{
		JsonHiveLoader loader(_REMOVE_NODE_TEST_HIVE_JSON);

		return HiveBuilder::build(loader, 1);
	}

	// Names of the nodes a node's edges lead to, in edge order.
	std::vector<std::string> _edgeTargets(GraphHive* hive, const std::string& nodeName)
	{
		std::vector<std::string> targets;

		Handle<GraphNode> node = hive -> getNode(nodeName);

		for(Handle<GraphEdge>& edge : node.getInstance() -> getEdges())
		{
			targets.push_back(edge.getInstance() -> traverse().getInstance() -> getName());
		}

		return targets;
	}

	bool _isStrobeEmitter(GraphHive* hive, const std::string& nodeName)
	{
		for(GraphHive::StrobeRegistration& registration : hive -> getStrobeEmitters())
		{
			if(registration.name == nodeName) return true;
		}

		return false;
	}
}

/**
 * Removing a node takes every edge into it from the other nodes with it, so nothing is left leading into a node
 * that is no longer part of the hive.
 */
TEST(GraphHiveRemoveNodeTest, RemovesInboundEdges)
{
	GraphHive* hive = _buildRemoveNodeTestHive();

	Handle<GraphNode> b = hive -> getNode("b");

	unsigned versionBefore = hive -> getVersion();

	ASSERT_TRUE(hive -> removeNode(b));

	EXPECT_GT(hive -> getVersion(), versionBefore);
	EXPECT_FALSE(hive -> getNode("b").isValid());
	EXPECT_TRUE(_edgeTargets(hive, "a").empty()) << "a's edge into b should have gone with b.";
	EXPECT_TRUE(_edgeTargets(hive, "c").empty()) << "c's edge into b should have gone with b.";
	EXPECT_TRUE(b.getInstance() -> getEdges().empty()) << "b's own edges should have gone with it.";

	EXPECT_FALSE(hive -> removeNode(b)) << "A node no longer in the hive cannot be removed again.";

	hive -> shutdown();
}

/**
 * A removed strobe emitter stops being one.
 */
TEST(GraphHiveRemoveNodeTest, ClearsStrobeEmitter)
{
	GraphHive* hive = _buildRemoveNodeTestHive();

	ASSERT_TRUE(_isStrobeEmitter(hive, "root"));

	ASSERT_TRUE(hive -> removeNode(hive -> getNode("root")));

	EXPECT_FALSE(_isStrobeEmitter(hive, "root"));

	hive -> shutdown();
}

/**
 * A replaced node's replacement takes its place: same name, every edge into and out of it carried over with its
 * restrictions, a self loop leading to the replacement, and the original decoupled.
 */
TEST(GraphHiveRemoveNodeTest, ReplaceCarriesEdges)
{
	GraphHive* hive = _buildRemoveNodeTestHive();

	Handle<GraphNode> original = hive -> getNode("b");

	PingNode* replacement = new PingNode();
	replacement -> setName("b");

	unsigned replacementId = replacement -> getId();

	ASSERT_TRUE(hive -> replaceNode(original, replacement));

	Handle<GraphNode> current = hive -> getNode("b");

	ASSERT_TRUE(current.isValid());
	EXPECT_EQ(current.getInstance() -> getId(), replacementId);
	EXPECT_TRUE(original.getInstance() -> getEdges().empty()) << "The original should be decoupled.";

	std::vector<std::string> outbound = _edgeTargets(hive, "b");
	std::sort(outbound.begin(), outbound.end());
	EXPECT_EQ(outbound, (std::vector<std::string>{"a", "b"}));

	for(Handle<GraphEdge>& edge : current.getInstance() -> getEdges())
	{
		Handle<GraphNode> toNode = edge.getInstance() -> traverse();

		if(toNode.getInstance() -> getName() == "b")
		{
			EXPECT_EQ(toNode.getInstance(), current.getInstance()) << "The self loop should lead to the replacement.";
			EXPECT_TRUE(edge.getInstance() -> getActionsCompleteAfterTraverse());
		}
	}

	std::vector<Handle<GraphEdge>> fromA = hive -> getNode("a").getInstance() -> getEdges();

	ASSERT_EQ(fromA.size(), 1u);
	EXPECT_EQ(fromA[0].getInstance() -> traverse().getInstance(), current.getInstance());
	EXPECT_EQ(fromA[0].getInstance() -> getActionFlags(), (unsigned long) PING_GRAPH_ACTION);
	EXPECT_EQ(fromA[0].getInstance() -> getTraversalKey(), 4u);

	EXPECT_EQ(_edgeTargets(hive, "c"), (std::vector<std::string>{"b"}));

	hive -> shutdown();
}

/**
 * A replacement under a different name is refused, and leaves the hive as it was.
 */
TEST(GraphHiveRemoveNodeTest, ReplaceRefusesDifferentName)
{
	GraphHive* hive = _buildRemoveNodeTestHive();

	Handle<GraphNode> original = hive -> getNode("b");

	PingNode* replacement = new PingNode();
	replacement -> setName("other");

	EXPECT_FALSE(hive -> replaceNode(original, replacement));

	EXPECT_EQ(hive -> getNode("b").getInstance(), original.getInstance());
	EXPECT_FALSE(hive -> getNode("other").isValid());
	EXPECT_EQ(_edgeTargets(hive, "c"), (std::vector<std::string>{"b"}));

	hive -> shutdown();
}

/**
 * A replaced strobe emitter's replacement keeps its registration and period.
 */
TEST(GraphHiveRemoveNodeTest, ReplaceKeepsStrobeEmitter)
{
	GraphHive* hive = _buildRemoveNodeTestHive();

	JsonHiveLoader loader(_REMOVE_NODE_TEST_HIVE_JSON);

	GraphNode* replacement = HiveBuilder::createNode(loader.getNode(3));

	ASSERT_TRUE(hive -> replaceNode(hive -> getNode("root"), replacement));

	std::vector<GraphHive::StrobeRegistration> emitters = hive -> getStrobeEmitters();

	ASSERT_EQ(emitters.size(), 1u);
	EXPECT_EQ(emitters[0].name, "root");
	EXPECT_EQ(emitters[0].periodMs, 1000u);

	hive -> shutdown();
}

#endif
