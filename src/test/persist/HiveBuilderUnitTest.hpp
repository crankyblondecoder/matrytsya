#ifndef HIVE_BUILDER_UNIT_TEST_H
#define HIVE_BUILDER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../util/Handle.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphHiveGraphViewSurface.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/actions/PingAction.hpp"
#include "../../graph/nodes/AgentNode.hpp"
#include "../../graph/nodes/PingNode.hpp"
#include "../../graph/nodes/SceneRootNode.hpp"
#include "../../graph/nodes/TriggerNode.hpp"
#include "../../persist/HiveBuilder.hpp"
#include "../../persist/HiveLoader.hpp"
#include "../../persist/HiveNodeDescriptor.hpp"
#include "../../persist/HiveSurfaceDescriptor.hpp"
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
			std::vector<HiveSurfaceDescriptor> surfaces;
			std::vector<std::pair<std::string, unsigned>> strobeEmitters;
			std::vector<std::pair<std::string, unsigned>> strobeSurfaces;

			std::string getHiveName() override {return hiveName;}
			unsigned getNodeCount() override {return nodes.size();}
			HiveNodeDescriptor getNode(unsigned index) override {return nodes[index];}
			unsigned getSurfaceCount() override {return surfaces.size();}
			HiveSurfaceDescriptor getSurface(unsigned index) override {return surfaces[index];}
			unsigned getStrobeEmitterCount() override {return strobeEmitters.size();}

			void getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& periodMs) override
			{
				nodeName = strobeEmitters[index].first;
				periodMs = strobeEmitters[index].second;
			}

			unsigned getStrobeSurfaceCount() override {return strobeSurfaces.size();}

			void getStrobeSurface(unsigned index, std::string& surfaceName, unsigned& periodMs) override
			{
				surfaceName = strobeSurfaces[index].first;
				periodMs = strobeSurfaces[index].second;
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

	HiveNodeDescriptor _makeAgentDescriptor(const std::string& name, const std::string& capabilityName,
		const std::string& promptNodeTypeName)
	{
		HiveNodeDescriptor descriptor{};
		descriptor.type = HiveNodeDescriptor::AGENT;
		descriptor.name = name;
		descriptor.capabilityName = capabilityName;

		HiveAgentPromptDescriptor prompt;
		prompt.nodeTypeName = promptNodeTypeName;
		prompt.prompt = "describe this node";

		descriptor.prompts.push_back(prompt);

		return descriptor;
	}

	HiveNodeDescriptor _makeTriggerDescriptor(const std::string& name)
	{
		HiveNodeDescriptor descriptor{};
		descriptor.type = HiveNodeDescriptor::TRIGGER;
		descriptor.name = name;
		descriptor.coreScript = "CORE_RAN = true";
		descriptor.pokeScript = "POKE_RAN = true";

		return descriptor;
	}

	HiveSurfaceDescriptor _makeSceneSurfaceDescriptor(const std::string& name, const std::string& sceneRootNodeName)
	{
		HiveSurfaceDescriptor descriptor{};
		descriptor.type = HiveSurfaceDescriptor::SCENE_SURFACE;
		descriptor.name = name;
		descriptor.sceneRootNodeName = sceneRootNodeName;

		return descriptor;
	}

	HiveSurfaceDescriptor _makeGraphViewSurfaceDescriptor(const std::string& name)
	{
		HiveSurfaceDescriptor descriptor{};
		descriptor.type = HiveSurfaceDescriptor::GRAPH_VIEW_SURFACE;
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
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> sourceHandle = hive -> getNode("source");
	Handle<GraphNode> targetHandle = hive -> getNode("target");

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
 * A strobe emitter registration with a period of 0 is rejected rather than silently ignored.
 */
TEST(HiveBuilderTest, StrobeEmitterPeriodZero_ThrowsInvalidStrobePeriod)
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

/**
 * A full, valid hive with a SceneRootNode, a scene surface bound to it, and a strobe surface
 * registration builds without throwing and the surface is retrievable by name.
 */
TEST(HiveBuilderTest, FullValidHiveWithSurface_BuildsAndRegistersStrobeSurface)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "root"));
	loader.strobeSurfaces.emplace_back("surface1", 5u);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(hive -> getSurface("surface1").isValid());

	hive -> shutdown();
}

/**
 * A graph view surface builds without being bound to any node, is retrievable by name as the graph view
 * surface it is, and is not mistaken for a scene surface.
 */
TEST(HiveBuilderTest, GraphViewSurface_BuildsWithoutABoundNode)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makePingDescriptor("a"));
	loader.surfaces.push_back(_makeGraphViewSurfaceDescriptor("structure"));
	loader.strobeSurfaces.emplace_back("structure", 5u);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(hive -> getSurface("structure").isValid());
	EXPECT_TRUE(hive -> getGraphViewSurface("structure").isValid());
	EXPECT_FALSE(hive -> getSceneSurface("structure").isValid());

	hive -> shutdown();
}

/**
 * A graph view surface reports the hive it was added to, i.e. every node in it along with the edges each
 * node has, once it has been strobed to catalogue it.
 */
TEST(HiveBuilderTest, GraphViewSurface_ReportsTheHivesNodesAndEdges)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor source = _makePingDescriptor("source");

	HiveEdgeDescriptor edge;
	edge.toNodeName = "target";
	edge.actionFlagNames.push_back("PING_GRAPH_ACTION");

	source.edges.push_back(edge);

	loader.nodes.push_back(source);
	loader.nodes.push_back(_makePingDescriptor("target"));
	loader.surfaces.push_back(_makeGraphViewSurfaceDescriptor("structure"));

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphHiveGraphViewSurface> surface = hive -> getGraphViewSurface("structure");

	ASSERT_TRUE(surface.isValid());

	surface.getInstance() -> strobe();

	GraphHiveGraphViewSurface::Graph graph = surface.getInstance() -> getGraph();

	ASSERT_EQ(graph.nodes.size(), 2u);

	// Catalogued in the order the nodes were added, which is the order they were declared in.
	EXPECT_EQ(graph.nodes[0].name, "source");
	EXPECT_EQ(graph.nodes[1].name, "target");

	ASSERT_EQ(graph.nodes[0].edges.size(), 1u);
	EXPECT_EQ(graph.nodes[0].edges[0].toNodeId, graph.nodes[1].id);

	EXPECT_EQ(graph.nodes[1].edges.size(), 0u);

	hive -> shutdown();
}

/**
 * An empty surface name is rejected.
 */
TEST(HiveBuilderTest, EmptySurfaceName_ThrowsInvalidSurfaceName)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("", "root"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * Two surfaces sharing the same name within one hive are rejected.
 */
TEST(HiveBuilderTest, DuplicateSurfaceNames_ThrowsDuplicateSurfaceName)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "root"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A surface whose referenced scene root node name does not exist among this hive's nodes is rejected.
 */
TEST(HiveBuilderTest, SurfaceReferencesMissingSceneRootNode_ThrowsSurfaceNodeNotFound)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "missing"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A surface whose referenced node exists but isn't a SceneRootNode is rejected.
 */
TEST(HiveBuilderTest, SurfaceReferencesWrongTypeNode_ThrowsSurfaceNodeWrongType)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makePingDescriptor("a"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "a"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A strobe surface registration with a period of 0 is rejected rather than silently ignored.
 */
TEST(HiveBuilderTest, StrobeSurfacePeriodZero_ThrowsInvalidStrobePeriod)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.surfaces.push_back(_makeSceneSurfaceDescriptor("surface1", "root"));
	loader.strobeSurfaces.emplace_back("surface1", 0u);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * A strobe surface registration referencing a surface name that doesn't exist is rejected.
 */
TEST(HiveBuilderTest, StrobeSurfaceTargetNotFound_ThrowsStrobeSurfaceNotFound)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeSceneRootDescriptor("root"));
	loader.strobeSurfaces.emplace_back("missing", 5u);

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * An agent node descriptor builds into an AgentNode carrying every field it was described with, and
 * its name-based capability and prompt node type are translated into their enum values.
 */
TEST(HiveBuilderTest, AgentNodeDescriptor_BuildsWithAllFields)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor descriptor = _makeAgentDescriptor("agent", "HIGH", "PING_NODE");
	descriptor.prompts[0].nodeIdentifier = "ping1";
	descriptor.prompts[0].terminateOnResponse = true;
	descriptor.autoTriggerAgentAction = false;

	loader.nodes.push_back(descriptor);

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> agentHandle = hive -> getNode("agent");

	ASSERT_TRUE(agentHandle.isValid());
	ASSERT_EQ(agentHandle.getInstance() -> getType(), GraphNodeType::AGENT_NODE);

	AgentNode* agentNode = static_cast<AgentNode*>(agentHandle.getInstance());

	EXPECT_EQ(agentNode -> getCapability(), AgenticHarness::Capability::HIGH);
	EXPECT_FALSE(agentNode -> getAutoTriggerAgentAction());

	ASSERT_EQ(agentNode -> getPrompts().size(), 1u);
	EXPECT_EQ(agentNode -> getPrompts()[0].nodeIdentifier, "ping1");
	EXPECT_EQ(agentNode -> getPrompts()[0].nodeType, GraphNodeType::PING_NODE);
	EXPECT_EQ(agentNode -> getPrompts()[0].prompt, "describe this node");
	EXPECT_TRUE(agentNode -> getPrompts()[0].terminateOnResponse);

	hive -> shutdown();
}

/**
 * A trigger node descriptor builds into a TriggerNode carrying its scripts, and emitTriggerOnPoke is
 * carried through rather than being left at the constructor's default.
 */
TEST(HiveBuilderTest, TriggerNodeDescriptor_BuildsWithAllFields)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor descriptor = _makeTriggerDescriptor("trigger");
	descriptor.emitTriggerOnPoke = false;

	loader.nodes.push_back(descriptor);

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> triggerHandle = hive -> getNode("trigger");

	ASSERT_TRUE(triggerHandle.isValid());
	ASSERT_EQ(triggerHandle.getInstance() -> getType(), GraphNodeType::TRIGGER_NODE);

	TriggerNode* triggerNode = static_cast<TriggerNode*>(triggerHandle.getInstance());

	EXPECT_FALSE(triggerNode -> getEmitTriggerOnPoke());

	hive -> shutdown();
}

/**
 * emitTriggerOnPoke defaults to true, so a descriptor that never mentions it still builds a node that
 * emits on being poked.
 */
TEST(HiveBuilderTest, TriggerNodeDescriptorWithoutEmitFlag_DefaultsToEmitting)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeTriggerDescriptor("trigger"));

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	Handle<GraphNode> triggerHandle = hive -> getNode("trigger");

	ASSERT_TRUE(triggerHandle.isValid());

	EXPECT_TRUE(static_cast<TriggerNode*>(triggerHandle.getInstance()) -> getEmitTriggerOnPoke());

	hive -> shutdown();
}

/**
 * An agent node whose capability name isn't one of the known capabilities is rejected.
 */
TEST(HiveBuilderTest, AgentNodeUnknownCapability_ThrowsUnknownAgentCapability)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeAgentDescriptor("agent", "EXTREME", "PING_NODE"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * An agent node prompt whose node type name isn't a known node type is rejected, rather than
 * silently becoming a prompt that can never match.
 */
TEST(HiveBuilderTest, AgentNodeUnknownPromptNodeType_ThrowsUnknownAgentPromptNodeType)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";
	loader.nodes.push_back(_makeAgentDescriptor("agent", "LOW", "NOT_A_NODE_TYPE"));

	EXPECT_THROW(HiveBuilder::build(loader, 1), PersistException);
}

/**
 * An edge can be restricted to the agent and trigger action flags, which an agent node's wiring
 * relies on.
 */
TEST(HiveBuilderTest, AgentAndTriggerActionFlags_AreRecognised)
{
	FakeHiveLoader loader;
	loader.hiveName = "Hive";

	HiveNodeDescriptor source = _makeAgentDescriptor("agent", "LOW", "PING_NODE");

	HiveEdgeDescriptor edge;
	edge.toNodeName = "target";
	edge.actionFlagNames.push_back("AGENT_GRAPH_ACTION");
	edge.actionFlagNames.push_back("TRIGGER_GRAPH_ACTION");
	source.edges.push_back(edge);

	loader.nodes.push_back(source);
	loader.nodes.push_back(_makePingDescriptor("target"));

	GraphHive* hive = HiveBuilder::build(loader, 1);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(hive -> getNode("agent").isValid());

	hive -> shutdown();
}

#endif
