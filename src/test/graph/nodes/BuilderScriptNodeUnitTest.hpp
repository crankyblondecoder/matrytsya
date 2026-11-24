#ifndef BUILDER_SCRIPT_NODE_UNIT_TEST_H
#define BUILDER_SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../../graph/actionTargets/TriggerActionTarget.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/GraphEdge.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/nodes/AgentNode.hpp"
#include "../../../graph/nodes/BuilderScriptNode.hpp"
#include "../../../graph/nodes/PingNode.hpp"
#include "../../../graph/nodes/SceneGeometryNode.hpp"
#include "../../../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/SceneTransformNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../graph/nodes/TriggerNode.hpp"
#include "../../../util/Handle.hpp"

namespace
{
	/**
	 * Build a hive named "BuilderHive" holding a builder named "builder" and a PingNode named "existing", so a
	 * builder script has something already in the hive to look up and wire to.
	 * @param coreScript Core script the builder runs.
	 * @param pokeScript Poke script the builder runs.
	 * @param builderOut Set to the builder. Owned by the hive.
	 * @param buildOnce Passed straight to the builder's constructor.
	 * @returns The hive, which the caller must shut down.
	 */
	GraphHive* _buildBuilderHive(const std::string& coreScript, const std::string& pokeScript,
		BuilderScriptNode*& builderOut, bool buildOnce = true)
	{
		GraphHive* hive = new GraphHive(2);
		hive -> setName("BuilderHive");

		builderOut = new BuilderScriptNode(coreScript, pokeScript, buildOnce);
		builderOut -> setName("builder");

		PingNode* existing = new PingNode();
		existing -> setName("existing");

		hive -> addNode(builderOut);
		hive -> addNode(existing);

		return hive;
	}

	/**
	 * Run a builder's core script once.
	 * @returns Whether the script ran without raising.
	 */
	bool _runBuilderCore(BuilderScriptNode* builder)
	{
		Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		return sessionHandle.getInstance() -> run();
	}

	/**
	 * Read a boolean global the builder's core script set.
	 * @returns The global, or false if it was never set to a boolean.
	 */
	bool _coreBool(BuilderScriptNode* builder, const char* name)
	{
		Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		bool value = false;

		if(!sessionHandle.getInstance() -> getGlobal(name, value)) return false;

		return value;
	}

	/**
	 * Read a string global the builder's core script set.
	 * @returns The global, or an empty string if it was never set to a string.
	 */
	std::string _coreString(BuilderScriptNode* builder, const char* name)
	{
		Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		const char* value = nullptr;

		if(!sessionHandle.getInstance() -> getGlobal(name, value) || !value) return "";

		return value;
	}
}

/**
 * A builder reports a type of its own, so it can be told apart from every other scripted node.
 */
TEST(BuilderScriptNodeTest, ReportsBuilderScriptNodeType)
{
	BuilderScriptNode* node = new BuilderScriptNode("", "");

	EXPECT_EQ(node -> getType(), GraphNodeType::BUILDER_SCRIPT_NODE);
	EXPECT_EQ(GraphNode::typeFromName("BUILDER_SCRIPT_NODE"), GraphNodeType::BUILDER_SCRIPT_NODE);

	node -> decrRef();
}

/**
 * Every create binding adds a node of its own type to the hive, under the name given, with the type specific
 * fields it was given applied to it.
 */
TEST(BuilderScriptNodeTest, CreatesEveryNodeTypeWithItsFields)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		local results = {
			createPingNode{ name = "ping", pokeEnabled = true },
			createTeleportNode{ name = "teleport", destination = { hiveName = "Other", nodeName = "target" } },
			createSceneRootNode{ name = "root", strobePeriodMs = 50 },
			createSceneGeometryNode{ name = "geometry", vertexGroups = {
				{ vertexes = { { posn = {0, 0, 0} }, { posn = {1, 0, 0}, colour = {255, 0, 0, 255} } } },
				{ visibility = VertexVisibility.HOVERED_OVER, vertexes = { { posn = {0, 1, 0} } } } } },
			createSceneGeometryScriptNode{ name = "geometryScript", coreScript = "", pokeScript = "",
				vertexGroups = { { vertexes = { { posn = {0, 0, 0} } } } } },
			createSceneTransformNode{ name = "transform",
				transform = {2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} },
			createSceneTransformScriptNode{ name = "transformScript" },
			createSceneTextScriptNode{ name = "text", text = "hello", textHeight = 2.5, textVisible = false,
				textColour = {1, 2, 3, 4}, textBackground = { padding = 0.5, cornerRadius = 0.25 },
				textChangeMode = "SCRIPT" },
			createAgentNode{ name = "agent", capability = "HIGH", autoTriggerAgentAction = false,
				prompts = { { nodeType = NodeType.TRIGGER_NODE, prompt = "go" } } },
			createTriggerNode{ name = "trigger", emitTriggerOnPoke = false }
		}

		allCreated = true
		for i = 1, 10 do allCreated = allCreated and results[i] == true end
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));
	EXPECT_TRUE(_coreBool(builder, "allCreated"));

	struct Expected
	{
		const char* name;
		GraphNodeType type;
	};

	const Expected expected[] =
	{
		{"ping", GraphNodeType::PING_NODE},
		{"teleport", GraphNodeType::TELEPORT_NODE},
		{"root", GraphNodeType::SCENE_ROOT_NODE},
		{"geometry", GraphNodeType::SCENE_GEOMETRY_NODE},
		{"geometryScript", GraphNodeType::SCENE_GEOMETRY_SCRIPT_NODE},
		{"transform", GraphNodeType::SCENE_TRANSFORM_NODE},
		{"transformScript", GraphNodeType::SCENE_TRANSFORM_SCRIPT_NODE},
		{"text", GraphNodeType::SCENE_TEXT_SCRIPT_NODE},
		{"agent", GraphNodeType::AGENT_NODE},
		{"trigger", GraphNodeType::TRIGGER_NODE}
	};

	for(const Expected& entry : expected)
	{
		Handle<GraphNode> node = hive -> getNode(entry.name);

		ASSERT_TRUE(node.isValid()) << entry.name << " should have been added to the hive.";
		EXPECT_EQ(node.getInstance() -> getType(), entry.type) << entry.name;
	}

	EXPECT_TRUE(hive -> getNode("ping").getInstance() -> getPokeEnabled());
	EXPECT_FALSE(hive -> getNode("teleport").getInstance() -> getPokeEnabled());

	Handle<GraphNode> geometry = hive -> getNode("geometry");
	EXPECT_EQ(dynamic_cast<SceneGeometryNode*>(geometry.getInstance()) -> getVertexCount(), 3u);

	Handle<GraphNode> geometryScript = hive -> getNode("geometryScript");
	EXPECT_EQ(dynamic_cast<SceneGeometryScriptNode*>(geometryScript.getInstance()) -> getVertexCount(), 1u);

	Handle<GraphNode> transform = hive -> getNode("transform");
	double matrix[16];
	dynamic_cast<SceneTransformNode*>(transform.getInstance()) -> getTransform(matrix);
	EXPECT_DOUBLE_EQ(matrix[0], 2.0);

	Handle<GraphNode> text = hive -> getNode("text");
	SceneTextScriptNode* textNode = dynamic_cast<SceneTextScriptNode*>(text.getInstance());
	EXPECT_EQ(textNode -> getText(), "hello");
	EXPECT_DOUBLE_EQ(textNode -> getHeight(), 2.5);
	EXPECT_FALSE(textNode -> getVisible());
	EXPECT_EQ(textNode -> getTextChangeMode(), SceneTextScriptNode::TextChangeMode::SCRIPT);
	EXPECT_DOUBLE_EQ(textNode -> getBackground().padding, 0.5);
	EXPECT_DOUBLE_EQ(textNode -> getBackground().cornerRadii[TextBackground::BOTTOM_LEFT], 0.25);

	Handle<GraphNode> agent = hive -> getNode("agent");
	AgentNode* agentNode = dynamic_cast<AgentNode*>(agent.getInstance());
	EXPECT_EQ(agentNode -> getCapability(), AgenticHarness::Capability::HIGH);
	EXPECT_FALSE(agentNode -> getAutoTriggerAgentAction());
	ASSERT_EQ(agentNode -> getPrompts().size(), 1u);
	EXPECT_EQ(agentNode -> getPrompts()[0].nodeType, GraphNodeType::TRIGGER_NODE);

	Handle<GraphNode> trigger = hive -> getNode("trigger");
	EXPECT_FALSE(dynamic_cast<TriggerNode*>(trigger.getInstance()) -> getEmitTriggerOnPoke());

	hive -> shutdown();
}

/**
 * createEdge wires any two nodes in the hive, in either direction between a new node and one that was already
 * there, carrying the action flags, traversal key and completion it was given.
 */
TEST(BuilderScriptNodeTest, CreatesEdgesInBothDirectionsWithOptions)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		createPingNode{ name = "fresh" }

		outOk = createEdge("fresh", "existing", { actionFlags = { ActionFlag.PING_GRAPH_ACTION,
			ActionFlag.TRIGGER_GRAPH_ACTION }, traversalKey = 7, actionsCompleteAfterTraverse = true })
		inOk = createEdge("existing", "fresh")
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));
	EXPECT_TRUE(_coreBool(builder, "outOk"));
	EXPECT_TRUE(_coreBool(builder, "inOk"));

	std::vector<Handle<GraphEdge>> freshEdges = hive -> getNode("fresh").getInstance() -> getEdges();

	ASSERT_EQ(freshEdges.size(), 1u);

	GraphEdge* outEdge = freshEdges[0].getInstance();

	EXPECT_EQ(outEdge -> getActionFlags(), static_cast<unsigned long>(PING_GRAPH_ACTION | TRIGGER_GRAPH_ACTION));
	EXPECT_EQ(outEdge -> getTraversalKey(), 7u);
	EXPECT_TRUE(outEdge -> getActionsCompleteAfterTraverse());
	EXPECT_EQ(outEdge -> traverse().getInstance() -> getName(), "existing");

	std::vector<Handle<GraphEdge>> existingEdges = hive -> getNode("existing").getInstance() -> getEdges();

	ASSERT_EQ(existingEdges.size(), 1u);

	GraphEdge* inEdge = existingEdges[0].getInstance();

	EXPECT_EQ(inEdge -> getActionFlags(), 0ul);
	EXPECT_EQ(inEdge -> getTraversalKey(), 0u);
	EXPECT_FALSE(inEdge -> getActionsCompleteAfterTraverse());
	EXPECT_EQ(inEdge -> traverse().getInstance() -> getName(), "fresh");

	hive -> shutdown();
}

/**
 * Something the hive cannot take, as it stands, is answered with false and a reason rather than an error, and
 * leaves the hive as it was.
 */
TEST(BuilderScriptNodeTest, RuntimeFailuresReturnFalseAndReason)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		local ok
		ok, duplicateReason = createPingNode{ name = "existing" }
		duplicateRefused = ok == false

		ok, missingReason = createEdge("existing", "nowhere")
		missingRefused = ok == false
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_TRUE(_coreBool(builder, "duplicateRefused"));
	EXPECT_NE(_coreString(builder, "duplicateReason").find("already exists"), std::string::npos);

	EXPECT_TRUE(_coreBool(builder, "missingRefused"));
	EXPECT_NE(_coreString(builder, "missingReason").find("nowhere"), std::string::npos);

	EXPECT_EQ(hive -> getNodeNames().size(), 2u) << "Nothing should have been added.";
	EXPECT_EQ(hive -> getNode("existing").getInstance() -> getEdges().size(), 0u);
	EXPECT_EQ(hive -> getNode("existing").getInstance() -> getType(), GraphNodeType::PING_NODE);

	hive -> shutdown();
}

/**
 * A malformed argument raises, and creates nothing: a misspelt field, a missing name, a value of the wrong type,
 * a constant that is not one, and a builder trying to create another builder.
 */
TEST(BuilderScriptNodeTest, MalformedArgumentsRaiseAndCreateNothing)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		local function raises(f, ...) return not pcall(f, ...) end

		unknownFieldRaised = raises(createPingNode, { name = "a", bogus = 1 })
		missingNameRaised = raises(createPingNode, { pokeEnabled = true })
		notTableRaised = raises(createPingNode, "a")
		wrongTypeRaised = raises(createSceneTextScriptNode, { name = "b", textHeight = "tall" })
		badHeightRaised = raises(createSceneTextScriptNode, { name = "b", textHeight = 0 })
		badTextChangeModeRaised = raises(createSceneTextScriptNode, { name = "b", textChangeMode = "direct" })
		badNodeTypeRaised = raises(createAgentNode, { name = "c", capability = "LOW",
			prompts = { { nodeType = 9999, prompt = "x" } } })
		badCapabilityRaised = raises(createAgentNode, { name = "c", capability = "ENORMOUS",
			prompts = { { nodeType = NodeType.PING_NODE, prompt = "x" } } })
		badVertexRaised = raises(createSceneGeometryNode, { name = "d",
			vertexGroups = { { vertexes = { { posn = {0, 0} } } } } })
		badFlagRaised = raises(createEdge, "existing", "builder", { actionFlags = { 3 } })
		badKeyRaised = raises(createEdge, "existing", "builder", { traversalKey = -1 })
		badEdgeOptionRaised = raises(createEdge, "existing", "builder", { colour = 1 })
		noBuilderBinding = createBuilderScriptNode == nil
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	const char* raisedGlobals[] = {"unknownFieldRaised", "missingNameRaised", "notTableRaised", "wrongTypeRaised",
		"badHeightRaised", "badTextChangeModeRaised", "badNodeTypeRaised", "badCapabilityRaised", "badVertexRaised",
		"badFlagRaised", "badKeyRaised", "badEdgeOptionRaised", "noBuilderBinding"};

	for(const char* name : raisedGlobals) EXPECT_TRUE(_coreBool(builder, name)) << name;

	EXPECT_EQ(hive -> getNodeNames().size(), 2u) << "Nothing should have been added.";
	EXPECT_EQ(hive -> getNode("existing").getInstance() -> getEdges().size(), 0u);

	hive -> shutdown();
}

/**
 * An uncaught malformed argument fails the script run as a whole, the same as any other Lua error.
 */
TEST(BuilderScriptNodeTest, UncaughtMalformedArgumentFailsTheRun)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive("createPingNode{ name = 'a', bogus = true }", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_FALSE(_runBuilderCore(builder));
	EXPECT_FALSE(hive -> getNode("a").isValid());

	hive -> shutdown();
}

/**
 * The lookup bindings describe the hive as it stands, including what the script itself just added to it.
 */
TEST(BuilderScriptNodeTest, LookupBindingsDescribeTheHive)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		hiveName = getHiveName()
		selfName = getSelfName()
		nodeCountBefore = #getNodeNames()
		existingExists = nodeExists("existing")
		missingExists = nodeExists("missing")
		missingInfoIsNil = getNodeInfo("missing") == nil

		createPingNode{ name = "fresh" }
		createEdge("existing", "fresh", { actionFlags = { ActionFlag.PING_GRAPH_ACTION } })

		local info = getNodeInfo("existing")
		infoName = info.name
		infoIsPing = info.type == NodeType.PING_NODE
		infoHasId = math.type(info.id) == "integer"
		infoEdgeCount = #info.edges
		infoEdgeTarget = info.edges[1].toNodeName
		infoEdgeTargetId = info.edges[1].toNodeId == getNodeInfo("fresh").id
		infoEdgeFlag = #info.edges[1].actionFlags == 1 and info.edges[1].actionFlags[1] == ActionFlag.PING_GRAPH_ACTION

		local pings = getNodesOfType(NodeType.PING_NODE)
		table.sort(pings)
		pingNames = table.concat(pings, ",")
		builderNames = table.concat(getNodesOfType(NodeType.BUILDER_SCRIPT_NODE), ",")
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_EQ(_coreString(builder, "hiveName"), "BuilderHive");
	EXPECT_EQ(_coreString(builder, "selfName"), "builder");
	EXPECT_TRUE(_coreBool(builder, "existingExists"));
	EXPECT_FALSE(_coreBool(builder, "missingExists"));
	EXPECT_TRUE(_coreBool(builder, "missingInfoIsNil"));
	EXPECT_EQ(_coreString(builder, "infoName"), "existing");
	EXPECT_TRUE(_coreBool(builder, "infoIsPing"));
	EXPECT_TRUE(_coreBool(builder, "infoHasId"));
	EXPECT_EQ(_coreString(builder, "infoEdgeTarget"), "fresh");
	EXPECT_TRUE(_coreBool(builder, "infoEdgeTargetId"));
	EXPECT_TRUE(_coreBool(builder, "infoEdgeFlag"));
	EXPECT_EQ(_coreString(builder, "pingNames"), "existing,fresh");
	EXPECT_EQ(_coreString(builder, "builderNames"), "builder");

	{ Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		int nodeCountBefore = 0;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("nodeCountBefore", nodeCountBefore));
		EXPECT_EQ(nodeCountBefore, 2);

		int infoEdgeCount = 0;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("infoEdgeCount", infoEdgeCount));
		EXPECT_EQ(infoEdgeCount, 1);
	}

	hive -> shutdown();
}

/**
 * The poke script is given the same bindings as the core script, so a poke can build too.
 */
TEST(BuilderScriptNodeTest, PokeScriptCanBuild)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive("", "createPingNode{ name = 'poked' } createEdge('poked', 'existing')",
		builder);
	Handle<GraphHive> hiveHandle(hive);

	{ Handle<ScriptSession> sessionHandle = builder -> requestPokeSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	Handle<GraphNode> poked = hive -> getNode("poked");

	ASSERT_TRUE(poked.isValid());
	EXPECT_EQ(poked.getInstance() -> getEdges().size(), 1u);

	hive -> shutdown();
}

/**
 * A builder that is not part of a hive has nowhere to put what it creates, which it says rather than raising.
 */
TEST(BuilderScriptNodeTest, BuilderOutsideAHiveReturnsFalse)
{
	BuilderScriptNode* builder = new BuilderScriptNode(R"(
		local ok
		ok, reason = createPingNode{ name = "orphan" }
		refused = ok == false
		hiveNameIsNil = getHiveName() == nil
		noNodes = #getNodeNames() == 0
	)", "");

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_TRUE(_coreBool(builder, "refused"));
	EXPECT_NE(_coreString(builder, "reason").find("not part of a hive"), std::string::npos);
	EXPECT_TRUE(_coreBool(builder, "hiveNameIsNil"));
	EXPECT_TRUE(_coreBool(builder, "noNodes"));

	builder -> decrRef();
}

/**
 * A builder that builds once only, which is the default, lets the session that built go on building for as long
 * as it lasts, then refuses every later attempt with a reason, leaving the hive as that first session left it.
 */
TEST(BuilderScriptNodeTest, BuildOnceRefusesASecondBuild)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		runs = (runs or 0) + 1
		nodeOk, nodeReason = createPingNode{ name = "node" .. runs }
		edgeOk, edgeReason = createEdge("existing", "builder")
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_TRUE(builder -> getBuildOnce());
	EXPECT_FALSE(builder -> getBuilt());

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_TRUE(_coreBool(builder, "nodeOk"));
	EXPECT_TRUE(_coreBool(builder, "edgeOk")) << "The session that built may go on building.";
	EXPECT_TRUE(builder -> getBuilt());

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_FALSE(_coreBool(builder, "nodeOk"));
	EXPECT_NE(_coreString(builder, "nodeReason").find("already built"), std::string::npos);
	EXPECT_FALSE(_coreBool(builder, "edgeOk"));
	EXPECT_NE(_coreString(builder, "edgeReason").find("already built"), std::string::npos);

	EXPECT_TRUE(hive -> getNode("node1").isValid());
	EXPECT_FALSE(hive -> getNode("node2").isValid());
	EXPECT_EQ(hive -> getNode("existing").getInstance() -> getEdges().size(), 1u) << "The edge should not have "
		"been built a second time.";

	hive -> shutdown();
}

/**
 * Only a session that actually builds uses the build up: one that only looks the hive up, or whose only attempt
 * raised for a malformed argument, leaves it for a later session.
 */
TEST(BuilderScriptNodeTest, BuildOnceIsOnlyUsedUpByASessionThatBuilds)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		if phase == "lookup" then
			seen = nodeExists("existing")
		elseif phase == "malformed" then
			raised = not pcall(createPingNode, { name = "x", bogus = true })
		else
			built = createPingNode{ name = "x" }
		end
	)", "", builder);
	Handle<GraphHive> hiveHandle(hive);

	const char* phases[] = {"lookup", "malformed"};

	for(const char* phase : phases)
	{
		{ Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

			sessionHandle.getInstance() -> setGlobal("phase", phase);
			ASSERT_TRUE(sessionHandle.getInstance() -> run());
		}

		EXPECT_FALSE(builder -> getBuilt()) << phase;
	}

	EXPECT_TRUE(_coreBool(builder, "seen"));
	EXPECT_TRUE(_coreBool(builder, "raised"));

	{ Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		sessionHandle.getInstance() -> setGlobal("phase", "build");
		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	EXPECT_TRUE(_coreBool(builder, "built"));
	EXPECT_TRUE(builder -> getBuilt());
	EXPECT_TRUE(hive -> getNode("x").isValid());

	hive -> shutdown();
}

/**
 * The build is shared by both scripts: once the core script has built, the poke script is refused too.
 */
TEST(BuilderScriptNodeTest, BuildOnceCoversThePokeScript)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive("createPingNode{ name = 'fromCore' }",
		"pokeOk, pokeReason = createPingNode{ name = 'fromPoke' }", builder);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	{ Handle<ScriptSession> sessionHandle = builder -> requestPokeSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());

		bool pokeOk = true;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("pokeOk", pokeOk));
		EXPECT_FALSE(pokeOk);
	}

	EXPECT_TRUE(hive -> getNode("fromCore").isValid());
	EXPECT_FALSE(hive -> getNode("fromPoke").isValid());

	hive -> shutdown();
}

/**
 * With buildOnce cleared, every session may build.
 */
TEST(BuilderScriptNodeTest, WithoutBuildOnceEverySessionBuilds)
{
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		runs = (runs or 0) + 1
		nodeOk = createPingNode{ name = "node" .. runs }
	)", "", builder, false);
	Handle<GraphHive> hiveHandle(hive);

	EXPECT_FALSE(builder -> getBuildOnce());

	ASSERT_TRUE(_runBuilderCore(builder));
	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_TRUE(_coreBool(builder, "nodeOk"));
	EXPECT_TRUE(hive -> getNode("node1").isValid());
	EXPECT_TRUE(hive -> getNode("node2").isValid());
	EXPECT_FALSE(builder -> getBuilt());

	hive -> shutdown();
}

namespace
{
	/**
	 * Build a hive holding a TriggerNode named "starter" with an edge to a builder named "builder", so a trigger
	 * emitted by the starter's core script reaches the builder as a real trigger action.
	 * @param starterScript Core script of the starter.
	 * @param builderScript Core script of the builder.
	 * @param starterOut Set to the starter. Owned by the hive.
	 * @param builderOut Set to the builder. Owned by the hive.
	 * @returns The hive, which the caller must shut down.
	 */
	GraphHive* _buildTriggeredBuilderHive(const std::string& starterScript, const std::string& builderScript,
		TriggerNode*& starterOut, BuilderScriptNode*& builderOut)
	{
		GraphHive* hive = new GraphHive(2);

		starterOut = new TriggerNode(starterScript, "");
		starterOut -> setName("starter");

		builderOut = new BuilderScriptNode(builderScript, "");
		builderOut -> setName("builder");

		hive -> addNode(starterOut);
		hive -> addNode(builderOut);

		Handle<GraphNode> builderHandle(builderOut);
		starterOut -> createEdge(builderHandle, {});

		return hive;
	}

	/**
	 * Run a starter's core script, then wait for every action it emitted to finish.
	 */
	void _fireStarter(GraphHive* hive, TriggerNode* starter)
	{
		{ Handle<ScriptSession> sessionHandle = starter -> requestCoreSession();

			ASSERT_TRUE(sessionHandle.getInstance() -> run());
		}

		hive -> waitOnNoActionsActive(0);
	}
}

/**
 * A builder is a trigger target: it supports the trigger flag and offers itself as the target.
 */
TEST(BuilderScriptNodeTest, IsATriggerTarget)
{
	BuilderScriptNode* node = new BuilderScriptNode("", "");

	EXPECT_EQ(node -> getTriggerActionTarget(), static_cast<TriggerActionTarget*>(node));
	EXPECT_TRUE(node -> getActionFlags() & TRIGGER_GRAPH_ACTION);

	node -> decrRef();
}

/**
 * A trigger action reaching a builder runs its core script, which builds.
 */
TEST(BuilderScriptNodeTest, TriggerRunsCoreScriptAndBuilds)
{
	TriggerNode* starter = nullptr;
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildTriggeredBuilderHive("trigger()",
		"createPingNode{ name = 'built' } createEdge('builder', 'built')", starter, builder);
	Handle<GraphHive> hiveHandle(hive);

	_fireStarter(hive, starter);

	Handle<GraphNode> built = hive -> getNode("built");

	ASSERT_TRUE(built.isValid()) << "The trigger should have run the builder's core script.";
	EXPECT_EQ(builder -> getEdges().size(), 1u);
	EXPECT_TRUE(builder -> getBuilt());

	hive -> shutdown();
}

/**
 * Each trigger is a session of its own, so with buildOnce set a second trigger runs the script but builds
 * nothing.
 */
TEST(BuilderScriptNodeTest, SecondTriggerIsRefusedByBuildOnce)
{
	TriggerNode* starter = nullptr;
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildTriggeredBuilderHive("trigger()", R"(
		runs = (runs or 0) + 1
		lastOk = createPingNode{ name = "built" .. runs }
	)", starter, builder);
	Handle<GraphHive> hiveHandle(hive);

	_fireStarter(hive, starter);
	_fireStarter(hive, starter);

	{ Handle<ScriptSession> sessionHandle = builder -> requestCoreSession();

		int runs = 0;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("runs", runs));
		EXPECT_EQ(runs, 2) << "Both triggers should have run the script.";

		bool lastOk = true;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("lastOk", lastOk));
		EXPECT_FALSE(lastOk);
	}

	EXPECT_TRUE(hive -> getNode("built1").isValid());
	EXPECT_FALSE(hive -> getNode("built2").isValid());

	hive -> shutdown();
}

/**
 * A trigger restricted to some other node passes the builder by without running its script.
 */
TEST(BuilderScriptNodeTest, RestrictedTriggerPassesBuilderBy)
{
	TriggerNode* starter = nullptr;
	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildTriggeredBuilderHive("trigger('someoneElse')", "createPingNode{ name = 'built' }",
		starter, builder);
	Handle<GraphHive> hiveHandle(hive);

	_fireStarter(hive, starter);

	EXPECT_FALSE(hive -> getNode("built").isValid());
	EXPECT_FALSE(builder -> getBuilt());

	hive -> shutdown();
}

/**
 * Adding a node under a name already in the hive is refused, even when nothing checked the name beforehand,
 * and the refused node's reference is released rather than leaked.
 */
TEST(BuilderScriptNodeTest, AddUniquelyNamedNodeRefusesATakenName)
{
	GraphHive* hive = new GraphHive(1);
	Handle<GraphHive> hiveHandle(hive);

	PingNode* first = new PingNode();
	first -> setName("same");

	EXPECT_TRUE(hive -> addUniquelyNamedNode(first));

	PingNode* second = new PingNode();
	second -> setName("same");

	// Held so the refused node can be inspected once the hive has let go of it.
	Handle<GraphNode> secondHandle(second);

	EXPECT_FALSE(hive -> addUniquelyNamedNode(second));
	EXPECT_FALSE(second -> getHive().isValid());

	PingNode* unnamed = new PingNode();

	EXPECT_FALSE(hive -> addUniquelyNamedNode(unnamed));

	EXPECT_EQ(hive -> getNodeNames().size(), 1u);
	EXPECT_EQ(hive -> getNode("same").getInstance(), static_cast<GraphNode*>(first));

	hive -> shutdown();
}

#endif
