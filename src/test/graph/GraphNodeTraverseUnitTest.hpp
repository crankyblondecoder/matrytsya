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

	/**
	 * Minimal action that holds a given set of edge traversal keys and matches PING_GRAPH_ACTION edges. It is
	 * never started; only edge selection is exercised.
	 */
	class KeyedTestAction : public GraphAction
	{
		public:

			KeyedTestAction(Handle<GraphNode> initNode, std::vector<unsigned> keys) :
				GraphAction(initNode, 32, 1, false, keys)
			{
				_addFlag(PING_GRAPH_ACTION, false);
			}

		protected:

			virtual bool _starting() override { return true; }
			virtual void _complete() override {}
			virtual bool _apply(GraphNode* node) override { return true; }
	};
}

/**
 * An action constructed with an edge's key may traverse that keyed edge, and it is still preferred by its
 * position in the edge array, as it would be without a key.
 */
TEST(GraphNodeTraverseTest, KeyedEdgeTakenByActionHoldingKey)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	Handle<GraphEdge> keyedEdge = fromNode -> createEdge(firstTarget, {}, 7);
	fromNode -> createEdge(secondTarget, {});

	KeyedTestAction* action = new KeyedTestAction(fromHandle, {3, 7});

	EXPECT_TRUE(fromNode -> traverse(*action) == keyedEdge) << "Keyed edge was not taken with its key.";

	action -> decrRef();
}

/**
 * A key matched edge is taken ahead of an action flag matched edge, even when the flagged edge sits earlier in
 * the edge array.
 */
TEST(GraphNodeTraverseTest, KeyMatchedEdgePreferredOverEarlierFlagMatchedEdge)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();
	Handle<GraphNode> thirdTarget = _makeNode();

	fromNode -> createEdge(firstTarget, {});
	fromNode -> createEdge(secondTarget, {PING_GRAPH_ACTION});
	Handle<GraphEdge> keyedEdge = fromNode -> createEdge(thirdTarget, {}, 7);

	KeyedTestAction* action = new KeyedTestAction(fromHandle, {7});

	EXPECT_TRUE(fromNode -> traverse(*action) == keyedEdge) << "Key matched edge was not preferred.";

	action -> decrRef();
}

/**
 * An edge matching both key and action flag is taken ahead of an earlier edge matching the key alone.
 */
TEST(GraphNodeTraverseTest, KeyAndFlagMatchedEdgePreferredOverEarlierKeyOnlyEdge)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	fromNode -> createEdge(firstTarget, {}, 7);
	Handle<GraphEdge> keyAndFlagEdge = fromNode -> createEdge(secondTarget, {PING_GRAPH_ACTION}, 7);

	KeyedTestAction* action = new KeyedTestAction(fromHandle, {7});

	EXPECT_TRUE(fromNode -> traverse(*action) == keyAndFlagEdge) << "Key and flag matched edge was not preferred.";

	action -> decrRef();
}

/**
 * Among equally specific edges, the earliest in the edge array is taken.
 */
TEST(GraphNodeTraverseTest, EarliestOfEquallySpecificKeyedEdgesTaken)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	Handle<GraphEdge> firstKeyedEdge = fromNode -> createEdge(firstTarget, {}, 3);
	fromNode -> createEdge(secondTarget, {}, 7);

	KeyedTestAction* action = new KeyedTestAction(fromHandle, {7, 3});

	EXPECT_TRUE(fromNode -> traverse(*action) == firstKeyedEdge) << "Earliest keyed edge was not taken.";

	action -> decrRef();
}

/**
 * Holding some keys does not open an edge locked with a different key.
 */
TEST(GraphNodeTraverseTest, KeyedEdgeBlocksActionHoldingOtherKeys)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> target = _makeNode();

	fromNode -> createEdge(target, {}, 7);

	KeyedTestAction* action = new KeyedTestAction(fromHandle, {3, 8});

	EXPECT_FALSE(fromNode -> traverse(*action).isValid()) << "Keyed edge was taken with the wrong key.";

	action -> decrRef();
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

/**
 * A keyed edge is passed over by an action that does not hold its key, even when the edge is flagged for
 * that action and would otherwise be preferred; the unkeyed sibling is taken instead.
 */
TEST(GraphNodeTraverseTest, KeyedEdgeSkippedByActionWithoutKey)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> firstTarget = _makeNode();
	Handle<GraphNode> secondTarget = _makeNode();

	fromNode -> createEdge(firstTarget, {PING_GRAPH_ACTION}, 7);
	Handle<GraphEdge> unkeyedEdge = fromNode -> createEdge(secondTarget, {});

	PingAction* action = new PingAction(fromHandle);

	EXPECT_TRUE(fromNode -> traverse(*action) == unkeyedEdge) << "Keyed edge was taken without its key.";

	action -> decrRef();
}

/**
 * With only a keyed edge available, an action without the key cannot traverse at all.
 */
TEST(GraphNodeTraverseTest, OnlyKeyedEdgeBlocksActionWithoutKey)
{
	Handle<GraphNode> fromHandle = _makeNode();
	GraphNode* fromNode = fromHandle.getInstance();

	Handle<GraphNode> target = _makeNode();

	fromNode -> createEdge(target, {}, 7);

	PingAction* action = new PingAction(fromHandle);

	EXPECT_FALSE(fromNode -> traverse(*action).isValid()) << "Keyed edge was taken without its key.";

	action -> decrRef();
}

#endif
