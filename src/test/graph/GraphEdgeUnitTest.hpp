#ifndef GRAPH_EDGE_UNIT_TEST_H
#define GRAPH_EDGE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/GraphEdge.hpp"
#include "../../graph/GraphHandle.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/graphActionFlagRegister.hpp"
#include "../../graph/nodes/PingNode.hpp"

namespace
{
	/**
	 * Build an edge pointing at a freshly created node, handing the caller ownership of the returned
	 * edge (refcount 1) without leaking the intermediate node handle's construction ref.
	 */
	GraphEdge* _makeEdge()
	{
		PingNode* targetNode = new PingNode();
		GraphHandle<GraphNode> targetHandle(targetNode);
		targetNode -> decrRef();

		return new GraphEdge(targetHandle);
	}
}

/**
 * An edge with no action flags added is unrestricted: any action flags, including none at all, can
 * traverse it.
 */
TEST(GraphEdgeTest, UnrestrictedEdgeCanTraverseAnyFlags)
{
	GraphEdge* edge = _makeEdge();

	EXPECT_TRUE(edge -> canTraverse(PING_GRAPH_ACTION));
	EXPECT_TRUE(edge -> canTraverse(SCRIPT_GRAPH_ACTION));
	EXPECT_TRUE(edge -> canTraverse(0));

	edge -> decrRef();
}

/**
 * An edge restricted to a single action flag can be traversed by that flag, whether it is passed
 * alone or alongside other, unrelated flags.
 */
TEST(GraphEdgeTest, RestrictedEdgeAllowsMatchingFlag)
{
	GraphEdge* edge = _makeEdge();
	edge -> addActionFlag(PING_GRAPH_ACTION);

	EXPECT_TRUE(edge -> canTraverse(PING_GRAPH_ACTION));
	EXPECT_TRUE(edge -> canTraverse(PING_GRAPH_ACTION | SCRIPT_GRAPH_ACTION));

	edge -> decrRef();
}

/**
 * An edge restricted to a single action flag rejects traversal by flags that don't include it, as
 * well as traversal with no flags at all.
 */
TEST(GraphEdgeTest, RestrictedEdgeBlocksNonMatchingFlag)
{
	GraphEdge* edge = _makeEdge();
	edge -> addActionFlag(PING_GRAPH_ACTION);

	EXPECT_FALSE(edge -> canTraverse(SCRIPT_GRAPH_ACTION));
	EXPECT_FALSE(edge -> canTraverse(0));

	edge -> decrRef();
}

/**
 * An edge restricted to several action flags can be traversed by any one of them individually, but
 * still rejects a flag outside that set.
 */
TEST(GraphEdgeTest, RestrictedEdgeWithMultipleFlagsAllowsAnyOfThem)
{
	GraphEdge* edge = _makeEdge();
	edge -> addActionFlag(PING_GRAPH_ACTION);
	edge -> addActionFlag(SCRIPT_GRAPH_ACTION);

	EXPECT_TRUE(edge -> canTraverse(PING_GRAPH_ACTION));
	EXPECT_TRUE(edge -> canTraverse(SCRIPT_GRAPH_ACTION));
	EXPECT_FALSE(edge -> canTraverse(SCENE_GRAPH_ACTION));

	edge -> decrRef();
}

#endif
