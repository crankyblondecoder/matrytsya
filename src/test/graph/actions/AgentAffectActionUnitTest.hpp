#ifndef AGENT_AFFECT_ACTION_UNIT_TEST_H
#define AGENT_AFFECT_ACTION_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/nodes/SceneGeometryNode.hpp"
#include "../../../util/Handle.hpp"

namespace
{
	/**
	 * Build a hive holding two SceneGeometryNodes connected by an edge restricted to
	 * AGENT_AFFECT_GRAPH_ACTION, with the upstream node built with the given emitAgentAffectAction flag.
	 * @param emitAgentAffectAction Passed straight to the upstream node's constructor.
	 * @param sourceOut Set to the upstream node built. Owned by the hive.
	 * @param downstreamOut Set to the downstream node built. Owned by the hive.
	 * @returns The hive, which the caller must shut down.
	 */
	GraphHive* _buildAgentVisibleHive(bool emitAgentAffectAction, SceneGeometryNode*& sourceOut,
		SceneGeometryNode*& downstreamOut)
	{
		GraphHive* hive = new GraphHive(2);

		sourceOut = new SceneGeometryNode(emitAgentAffectAction);
		downstreamOut = new SceneGeometryNode();

		hive -> addNode(sourceOut);
		hive -> addNode(downstreamOut);

		Handle<GraphNode> downstreamHandle(downstreamOut);
		sourceOut -> createEdge(downstreamHandle, {AGENT_AFFECT_GRAPH_ACTION});

		return hive;
	}
}

/**
 * A SceneGeometryNode built with emitAgentAffectAction false only ever changes its own agent visible
 * flag. Nothing is emitted, so a downstream node connected to it never hears about the change.
 */
TEST(AgentAffectActionTest, SetAgentVisibleWithEmitDisabled_DoesNotReachDownstreamNode)
{
	SceneGeometryNode* source = nullptr;
	SceneGeometryNode* downstream = nullptr;

	GraphHive* hive = _buildAgentVisibleHive(false, source, downstream);
	Handle<GraphHive> hiveHandle(hive);

	source -> agentAffectingStart(true);

	hive -> waitOnNoActionsActive(0);

	EXPECT_TRUE(source -> getAgentVisible());
	EXPECT_FALSE(downstream -> getAgentVisible()) << "No AgentAffectAction should have been emitted while "
		"emitAgentAffectAction is false.";

	hive -> shutdown();
}

/**
 * A SceneGeometryNode built with emitAgentAffectAction true emits an AgentAffectAction of its own
 * whenever its agent visible flag changes, carrying the new value onward to every AgentAffectActionTarget
 * it reaches.
 */
TEST(AgentAffectActionTest, SetAgentVisibleWithEmitEnabled_PropagatesToDownstreamNode)
{
	SceneGeometryNode* source = nullptr;
	SceneGeometryNode* downstream = nullptr;

	GraphHive* hive = _buildAgentVisibleHive(true, source, downstream);
	Handle<GraphHive> hiveHandle(hive);

	source -> agentAffectingStart(true);

	hive -> waitOnNoActionsActive(0);

	EXPECT_TRUE(downstream -> getAgentVisible()) << "The AgentAffectAction emitted by the source node "
		"should have reached the downstream node.";

	source -> agentAffectingEnd(true);

	hive -> waitOnNoActionsActive(0);

	EXPECT_FALSE(downstream -> getAgentVisible()) << "Clearing the flag should propagate the same way as "
		"setting it.";

	hive -> shutdown();
}

#endif
