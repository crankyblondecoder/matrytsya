#ifndef SCRIPT_ACTION_UNIT_TEST_H
#define SCRIPT_ACTION_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/actions/ScriptAction.hpp"
#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/GraphSerialisedActionNode.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/graphSceneElements.hpp"
#include "../../../graph/nodes/PingNode.hpp"
#include "../../../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"

/**
 * Graph node that emits a ScriptAction of its own, mirroring how PingNode exposes emitPing().
 */
class ScriptEmitterNode : public GraphSerialisedActionNode
{
	public:

		virtual ~ScriptEmitterNode() {}

		ScriptEmitterNode() : GraphSerialisedActionNode()
		{
			_setEnergyCost(1);
		}

		/**
		 * Construct and emit a plain script action from this node.
		 * @param wait Wait for action to complete.
		 * @returns Script action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		ScriptAction* emitScript(bool wait)
		{
			Handle<GraphNode> handle(this);

			return emit(new ScriptAction(handle), wait);
		}

		/**
		 * Emit an already constructed script action (e.g. a ScriptAction subclass bound to this node).
		 * @param action Action to emit.
		 * @param wait Wait for action to complete.
		 * @returns The same action, refincr'd. Caller must decref this to dispose.
		 */
		ScriptAction* emit(ScriptAction* action, bool wait)
		{
			action -> incrRef();

			_emitAction(action);

			if(wait) action -> waitOnComplete(0);

			return action;
		}

	protected:

		void _poked(GraphPoke poke) override {}
};

/**
 * ScriptAction subclass that publishes a value every node can read, exercising _shareGlobal() as the one
 * explicit, action-decided channel for state to cross between nodes.
 */
class SharingScriptAction : public ScriptAction
{
	public:

		SharingScriptAction(Handle<GraphNode>& initNode) : ScriptAction(initNode)
		{
			_shareGlobal("shared", 99);
		}
};

/**
 * ScriptAction subclass that carries a running integer counter through a chain of plain ScriptNode
 * instances. Per-node isolation means a node's own update to "counter" never reaches the next node (or this
 * action) on its own, so this reads back whatever it last shared via _getGlobal(), advances it itself, and
 * re-publishes it via _shareGlobal() before the next node runs.
 */
class AccumulatingScriptAction : public ScriptAction
{
	public:

		/**
		 * @param initNode Initial node this action is bound to.
		 * @param startValue Value "counter" is seeded with before the first node in the chain is invoked.
		 */
		AccumulatingScriptAction(Handle<GraphNode>& initNode, int startValue) : ScriptAction(initNode)
		{
			_shareGlobal("counter", startValue);
		}

		/**
		 * Get the counter's current shared value.
		 */
		int getCounter()
		{
			int value = 0;
			_getGlobal("counter", value);
			return value;
		}

	protected:

		void _apply(GraphNode* target) override
		{
			ScriptAction::_apply(target);

			int value = 0;

			// This makes the updated global from the last node application available to the next script node.
			if(_getGlobal("counter", value)) _shareGlobal("counter", value);
		}
};

/**
 * ScriptAction subclass that captures a SceneGeometryScriptNode script's vertexCount() readings, published as
 * globals, so the test can assert on them directly rather than via script-side error() checks.
 */
class VertexCountCapturingScriptAction : public ScriptAction
{
	public:

		VertexCountCapturingScriptAction(Handle<GraphNode>& initNode) : ScriptAction(initNode) {}

		int getCountBefore()
		{
			int value = -1;
			_getGlobal("countBefore", value);
			return value;
		}

		int getCountAfterOne()
		{
			int value = -1;
			_getGlobal("countAfterOne", value);
			return value;
		}

		int getCountAfterMany()
		{
			int value = -1;
			_getGlobal("countAfterMany", value);
			return value;
		}
};

TEST(ScriptActionTest, GlobalsAreIsolatedPerNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();
	ScriptNode* writerNode = new ScriptNode("leaked = 123", "");
	ScriptNode* readerNode = new ScriptNode("leakedWasNil = (leaked == nil)", "");

	hive -> addNode(sourceNode);
	hive -> addNode(writerNode);
	hive -> addNode(readerNode);

	Handle<GraphNode> writerHandle(writerNode);
	Handle<GraphNode> readerHandle(readerNode);

	sourceNode -> createEdge(writerHandle, {});
	writerNode -> createEdge(readerHandle, {});

	ScriptAction* action = sourceNode -> emitScript(true);

	{ Handle<ScriptSession> sessionHandle = readerNode -> requestCoreSession();

		bool wasNil = false;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("leakedWasNil", wasNil))
			<< "Reader node's script was never invoked.";

		EXPECT_TRUE(wasNil)
			<< "Global set by one node's script should not be visible to the next node's script.";
	}

	action -> decrRef();

	hive -> shutdown();
}

TEST(ScriptActionTest, ExplicitlySharedGlobalsAreVisibleToEveryNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();
	ScriptNode* readerNode1 = new ScriptNode("observedShared = shared", "");
	ScriptNode* readerNode2 = new ScriptNode("observedShared = shared", "");

	hive -> addNode(sourceNode);
	hive -> addNode(readerNode1);
	hive -> addNode(readerNode2);

	Handle<GraphNode> reader1Handle(readerNode1);
	Handle<GraphNode> reader2Handle(readerNode2);

	sourceNode -> createEdge(reader1Handle, {});
	readerNode1 -> createEdge(reader2Handle, {});

	Handle<GraphNode> sourceHandle(sourceNode);
	SharingScriptAction* action = new SharingScriptAction(sourceHandle);

	sourceNode -> emit(action, true);

	int value = -1;

	{ Handle<ScriptSession> sessionHandle = readerNode1 -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("observedShared", value))
			<< "Reader node 1's script was never invoked.";
		EXPECT_EQ(value, 99) << "Explicitly shared global should be visible to every node.";
	}

	{ Handle<ScriptSession> sessionHandle = readerNode2 -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("observedShared", value))
			<< "Reader node 2's script was never invoked.";
		EXPECT_EQ(value, 99) << "Explicitly shared global should be visible to every node.";
	}

	action -> decrRef();

	hive -> shutdown();
}

TEST(ScriptActionTest, ScriptNodesAccumulateCounter)
{
	// Non-cyclic graph of 5 nodes: a PingNode root (the action's initial node, never itself invoked) followed
	// by a chain of 4 plain ScriptNode instances. Each one reads "counter" and adds an integer value to it, and the action
	// re-shares the result between nodes via _shareGlobal()/_getGlobal(),

	GraphHive* hive = new GraphHive(2);

	Handle<GraphHive> hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	PingNode* rootNode = new PingNode();
	ScriptNode* node1 = new ScriptNode("counter = counter + 3", "");
	ScriptNode* node2 = new ScriptNode("counter = counter + 4", "");
	ScriptNode* node3 = new ScriptNode("counter = counter + 5", "");
	ScriptNode* node4 = new ScriptNode("counter = counter + 6", "");

	hive -> addNode(rootNode);
	hive -> addNode(node1);
	hive -> addNode(node2);
	hive -> addNode(node3);
	hive -> addNode(node4);

	Handle<GraphNode> node1Handle(node1);
	Handle<GraphNode> node2Handle(node2);
	Handle<GraphNode> node3Handle(node3);
	Handle<GraphNode> node4Handle(node4);

	rootNode -> createEdge(node1Handle, {});
	node1 -> createEdge(node2Handle, {});
	node2 -> createEdge(node3Handle, {});
	node3 -> createEdge(node4Handle, {});

	Handle<GraphNode> rootHandle(rootNode);

	AccumulatingScriptAction* action = new AccumulatingScriptAction(rootHandle, 1);

	action -> incrRef();

	action -> start();
	action -> waitOnComplete(0);

	EXPECT_EQ(action -> getCounter(), 1 + 3 + 4 + 5 + 6) << "Final accumulated counter did not match expected value.";

	action -> decrRef();

	hive -> shutdown();
}

TEST(ScriptActionTest, SceneGeometryScriptNodeExposesVertexToLua)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();

	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertex(Vertex{"
		"	posn = {1, 2, 3},"
		"	colour = {10, 20, 30, 40},"
		"	texCoords = {0.5, 0.6},"
		"	normal = {0, 0, 1}"
		"})", "");

	hive -> addNode(sourceNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);

	sourceNode -> createEdge(geometryHandle, {});

	ScriptAction* action = sourceNode -> emitScript(true);

	action -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(0));

	surface -> setHive(hiveHandle);
	Handle<GraphNode> sourceHandle(sourceNode);
	SceneAction* sceneAction = new SceneAction(sourceHandle, Handle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);

	const Vertex& vertex = chunks[0].vertexes[0];

	EXPECT_DOUBLE_EQ(vertex.posn[0], 1);
	EXPECT_DOUBLE_EQ(vertex.posn[1], 2);
	EXPECT_DOUBLE_EQ(vertex.posn[2], 3);

	EXPECT_EQ(std::to_integer<int>(vertex.colour[0]), 10);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[1]), 20);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[2]), 30);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[3]), 40);

	EXPECT_DOUBLE_EQ(vertex.texCoords[0], 0.5);
	EXPECT_DOUBLE_EQ(vertex.texCoords[1], 0.6);

	EXPECT_DOUBLE_EQ(vertex.normal[0], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[1], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[2], 1);

	sceneAction -> decrRef();

	surface -> close();

	hive -> shutdown();
}

TEST(ScriptActionTest, SceneGeometryScriptNodeExposesAddVertexesToLua)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();

	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertexes({"
		"	Vertex{posn = {1, 0, 0}},"
		"	Vertex{posn = {0, 1, 0}},"
		"	Vertex{posn = {0, 0, 1}}"
		"})", "");

	hive -> addNode(sourceNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);

	sourceNode -> createEdge(geometryHandle, {});

	ScriptAction* action = sourceNode -> emitScript(true);

	action -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(0));

	surface -> setHive(hiveHandle);
	Handle<GraphNode> sourceHandle(sourceNode);
	SceneAction* sceneAction = new SceneAction(sourceHandle, Handle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 3u);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[0], 1);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[1], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[2], 0);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[0], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[1], 1);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[2], 0);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[0], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[1], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[2], 1);

	sceneAction -> decrRef();

	surface -> close();

	hive -> shutdown();
}

TEST(ScriptActionTest, SceneGeometryScriptNodeGroupsVertexesByVisibility)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();

	// A default (ALWAYS) vertex followed by a GRABBED one; the differing visibility should split them
	// into two separate chunks.
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertex(Vertex{posn = {1, 0, 0}})\n"
		"addVertex(Vertex{posn = {0, 1, 0}}, VertexVisibility.GRABBED)\n", "");

	hive -> addNode(sourceNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);

	sourceNode -> createEdge(geometryHandle, {});

	ScriptAction* action = sourceNode -> emitScript(true);

	action -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(0));

	surface -> setHive(hiveHandle);
	Handle<GraphNode> sourceHandle(sourceNode);
	SceneAction* sceneAction = new SceneAction(sourceHandle, Handle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 2u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);
	ASSERT_EQ(chunks[1].vertexes.size(), 1u);

	EXPECT_EQ(chunks[0].visibility, SceneGeometry::VertexVisibility::ALWAYS)
		<< "A vertex added without a visibility argument should default to ALWAYS.";
	EXPECT_EQ(chunks[1].visibility, SceneGeometry::VertexVisibility::GRABBED)
		<< "A vertex added with VertexVisibility.GRABBED should land in a GRABBED chunk.";

	sceneAction -> decrRef();

	surface -> close();

	hive -> shutdown();
}

TEST(ScriptActionTest, SceneGeometryScriptNodeRejectsUnknownVisibility)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();

	// 999 is not a valid VertexVisibility value, so addVertex() should raise a Lua error and abort the
	// script before the vertex is stored.
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertex(Vertex{posn = {1, 2, 3}}, 999)\n", "");

	hive -> addNode(sourceNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);

	sourceNode -> createEdge(geometryHandle, {});

	ScriptAction* action = sourceNode -> emitScript(true);

	action -> decrRef();

	EXPECT_EQ(geometryNode -> getVertexCount(), 0u)
		<< "An unrecognized VertexVisibility value should raise a Lua error and add no vertexes.";

	hive -> shutdown();
}

TEST(ScriptActionTest, SceneGeometryScriptNodeExposesVertexCountToLua)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();

	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"countBefore = vertexCount()\n"
		"addVertex(Vertex{posn = {1, 2, 3}})\n"
		"countAfterOne = vertexCount()\n"
		"addVertexes({Vertex{posn = {4, 5, 6}}, Vertex{posn = {7, 8, 9}}})\n"
		"countAfterMany = vertexCount()\n", "");

	hive -> addNode(sourceNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);

	sourceNode -> createEdge(geometryHandle, {});

	Handle<GraphNode> sourceHandle(sourceNode);
	VertexCountCapturingScriptAction* action = new VertexCountCapturingScriptAction(sourceHandle);

	sourceNode -> emit(action, true);

	EXPECT_EQ(action -> getCountBefore(), 0) << "Vertex count should start at zero.";
	EXPECT_EQ(action -> getCountAfterOne(), 1) << "Vertex count should reflect a single addVertex() call.";
	EXPECT_EQ(action -> getCountAfterMany(), 3) << "Vertex count should reflect a following addVertexes() call.";

	action -> decrRef();

	hive -> shutdown();
}

#endif
