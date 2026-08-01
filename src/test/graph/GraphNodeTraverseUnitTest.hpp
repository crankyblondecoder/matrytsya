#ifndef GRAPH_NODE_TRAVERSE_UNIT_TEST_H
#define GRAPH_NODE_TRAVERSE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../graph/actions/PingAction.hpp"
#include "../../graph/GraphEdge.hpp"
#include "../../util/Handle.hpp"
#include "../../graph/graphActionFlagRegister.hpp"
#include "../../graph/GraphNode.hpp"
#include "../../graph/nodes/PingNode.hpp"

namespace
{
	/**
	 * Create a node, handing the caller the only reference to it.
	 * Which node an edge reaches doesn't matter to any of these tests, only which edge traverse() picks,
	 * so every edge built below points at one of these.
	 */
	Handle<GraphNode> _makeNode()
	{
		PingNode* node = new PingNode();
		Handle<GraphNode> nodeHandle(node);

		// The handle holds the reference; release the implicit construction ref.
		node -> decrRef();

		return nodeHandle;
	}
}

/**
 * An edge that names the actions it accepts is taken ahead of an unflagged wildcard edge, even when the
 * wildcard sits earlier in the node's edge array and would have been picked on array order alone.
 */
TEST(GraphNodeTraverseTest, FlaggedEdgePreferredOverEarlierWildcardEdge)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();
	Handle<GraphNode> thirdTarget = _makeNode();

	fromNode -> createEdge(firstTarget, {});
	Handle<GraphEdge> flaggedEdge = fromNode -> createEdge(secondTarget, {PING_GRAPH_ACTION});
	fromNode -> createEdge(thirdTarget, {});

	PingAction* action = new PingAction(fromHandle);

	EXPECT_TRUE(fromNode -> traverse(*action) == flaggedEdge) << "Flagged edge was not preferred.";

	action -> decrRef();
}

/**
 * With no flagged edge available, the first traversable wildcard edge in array order is still taken.
 */
TEST(GraphNodeTraverseTest, FirstWildcardEdgeTakenWhenNoEdgeIsFlagged)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	Handle<GraphEdge> firstEdge = fromNode -> createEdge(firstTarget, {});
	fromNode -> createEdge(secondTarget, {});

	PingAction* action = new PingAction(fromHandle);

	EXPECT_TRUE(fromNode -> traverse(*action) == firstEdge) << "First wildcard edge was not taken.";

	action -> decrRef();
}

/**
 * The preference only applies to edges the action can actually traverse: a flagged edge that excludes this
 * action is passed over for the wildcard, rather than blocking traversal.
 */
TEST(GraphNodeTraverseTest, WildcardEdgeTakenWhenFlaggedEdgeExcludesTheAction)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	fromNode -> createEdge(firstTarget, {SCRIPT_GRAPH_ACTION});
	Handle<GraphEdge> wildcardEdge = fromNode -> createEdge(secondTarget, {});

	PingAction* action = new PingAction(fromHandle);

	EXPECT_TRUE(fromNode -> traverse(*action) == wildcardEdge) << "Wildcard edge was not fallen back to.";

	action -> decrRef();
}

#endif
