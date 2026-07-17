#ifndef HIVE_BUILDER_UNIT_TEST_H
#define HIVE_BUILDER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/GraphHandle.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/actions/PingAction.hpp"
#include "../../graph/nodes/PingNode.hpp"
#include "../../graph/nodes/SceneRootNode.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/HiveLoader.hpp"
#include "../../persist/HiveNodeDescriptor.hpp"
#include "../../persist/PersistException.hpp"

#include <string>
#include <utility>
#include <vector>

namespace
{
	/**
	 * In-test HiveLoader whose data is populated directly by test cases, so HiveBuilder's own logic
	 * can be exercised independent of any particular persisted format.
	 */
	class FakeHiveLoader : public HiveLoader
	{
		public:

			std::string hiveName;
			std::vector<HiveNodeDescriptor> nodes;
			std::vector<std::pair<std::string, unsigned>> strobeEmitters;

			std::string getHiveName() override {return hiveName;}
			unsigned getNodeCount() override {return nodes.size();}
			HiveNodeDescriptor getNode(unsigned index) override {return nodes[index];}
			unsigned getStrobeEmitterCount() override {return strobeEmitters.size();}

			void getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& frequencyHz) override
			{
				nodeName = strobeEmitters[index].first;
				frequencyHz = strobeEmitters[index].second;
			}
	};

	HiveNodeDescriptor _makePingDescriptor(const std::string& name)
	{
		HiveNodeDescriptor descriptor{};
		descriptor.type = HiveNodeDescriptor::PING;
		descriptor.name = name;

		return descriptor;
	}

	HiveNodeDescriptor _makeSceneRootDescriptor(const std::string& name)
	{
		HiveNodeDescriptor descriptor{};
		descriptor.type = HiveNodeDescriptor::SCENE_ROOT;
		descriptor.name = name;

		return descriptor;
	}
}

/**
 * A full, valid hive (two connected PingNodes plus a SceneRootNode strobe emitter) builds without
 * throwing, resolves nodes by name, and actually wires the edge: a ping emitted from the source node
 * is received by the target node, whether the edge is unrestricted or restricted to a matching flag.
 */
TEST(HiveBuilderTest, FullValidHive_BuildsAndWiresCorrectly)
{
	FakeHiveLoader loader;
	loader.hiveName = "TestHive";

	HiveNodeDescriptor source = _makePingDescriptor("source");

	HiveEdgeDescriptor edge;
	edge.toNodeName = "target";
	edge.actionFlagNames.push_back("PING_GRAPH_ACTION");
	source.edges.push_back(edge);

	loader.nodes.push_back(source);
	loader.nodes.push_back(_makePingDescriptor("target"));
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));

	loader.strobeEmitters.emplace_back("root", 5u);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	GraphHandle<GraphHive> hiveHandle(hive);

	GraphHandle<GraphNode> sourceHandle = hive -> getNode("source");
	GraphHandle<GraphNode> targetHandle = hive -> getNode("target");

	ASSERT_TRUE(sourceHandle.isValid());
	ASSERT_TRUE(targetHandle.isValid());

	PingNode* sourceNode = dynamic_cast<PingNode*>(sourceHandle.getInstance());
	PingNode* targetNode = dynamic_cast<PingNode*>(targetHandle.getInstance());

	ASSERT_NE(sourceNode, nullptr);
	ASSERT_NE(targetNode, nullptr);

	PingAction* action = sourceNode -> emitPing(true);

	EXPECT_EQ(targetNode -> getPingCount(), 1u);

	action -> decrRef();

	hive -> shutdown();
}

/**
 * An empty hive name is rejected.
 */
TEST(HiveBuilderTest, EmptyHiveName_ThrowsInvalidHiveName)
{
	FakeHiveLoader loader;
	loader.hiveName = "";
	loader.nodes.push_back(_makePingDescriptor("a"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A hive with no nodes at all is rejected.
 */
TEST(HiveBuilderTest, NoNodes_ThrowsNoNodes)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * Two nodes sharing the same name within one hive are rejected.
 */
TEST(HiveBuilderTest, DuplicateNodeNames_ThrowsDuplicateNodeName)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makePingDescriptor("a"));
	loader.nodes.push_back(_makePingDescriptor("a"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * An edge whose "toNodeName" name does not match any node in the hive is rejected.
 */
TEST(HiveBuilderTest, EdgeTargetNotFound_ThrowsEdgeTargetNotFound)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor a = _makePingDescriptor("a");

	HiveEdgeDescriptor edge;
	edge.toNodeName = "missing";
	a.edges.push_back(edge);

	loader.nodes.push_back(a);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * An edge action flag name that isn't in the action flag register is rejected.
 */
TEST(HiveBuilderTest, UnknownActionFlagName_ThrowsUnknownActionFlag)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor a = _makePingDescriptor("a");

	HiveEdgeDescriptor edge;
	edge.toNodeName = "b";
	edge.actionFlagNames.push_back("NOT_A_REAL_FLAG");
	a.edges.push_back(edge);

	loader.nodes.push_back(a);
	loader.nodes.push_back(_makePingDescriptor("b"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A strobe emitter registration with a frequency of 0 is rejected rather than silently ignored.
 */
TEST(HiveBuilderTest, StrobeEmitterFrequencyZero_ThrowsInvalidStrobeFrequency)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.strobeEmitters.emplace_back("root", 0u);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A strobe emitter registration referencing a node name that doesn't exist is rejected.
 */
TEST(HiveBuilderTest, StrobeEmitterTargetNotFound_ThrowsStrobeEmitterNotFound)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.strobeEmitters.emplace_back("missing", 5u);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A strobe emitter registration referencing a node that isn't a StrobeEmitterNode subclass is
 * rejected rather than silently ignored.
 */
TEST(HiveBuilderTest, StrobeEmitterOnNonStrobeEmitterNode_ThrowsStrobeEmitterWrongType)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makePingDescriptor("a"));
	loader.strobeEmitters.emplace_back("a", 5u);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

#endif
