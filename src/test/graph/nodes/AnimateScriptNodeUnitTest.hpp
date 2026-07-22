#ifndef ANIMATE_SCRIPT_NODE_UNIT_TEST_H
#define ANIMATE_SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actionTargets/AnimateActionTarget.hpp"
#include "../../../graph/actionTargets/ScriptActionTarget.hpp"
#include "../../../graph/actions/ScriptAction.hpp"
#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/nodes/AnimateScriptNode.hpp"

/**
 * AnimateScriptNode never overrides StrobeActionTarget::strobe() (inherited via StrobeScriptNode), leaving
 * it abstract; this gives every test a concrete, otherwise-untouched instance to exercise.
 */
class TestAnimateScriptNode : public AnimateScriptNode
{
	public:

		TestAnimateScriptNode(const std::string& coreScript, const std::string& pokeScript)
			: AnimateScriptNode(coreScript, pokeScript) {}

		void strobe() override {}
};

/**
 * A node's animating flag lives outside the per-invoke Lua environment (it is node state, flipped either
 * directly via setAnimating() or by an AnimateAction) and defaults to false until something changes it.
 */
TEST(AnimateScriptNodeTest, DefaultAnimatingStateIsFalse)
{
	TestAnimateScriptNode* node = new TestAnimateScriptNode("seenAnimating = getAnimating()", "");

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());

	bool seenAnimating = true;
	ASSERT_TRUE(target -> getGlobal("seenAnimating", seenAnimating));
	EXPECT_FALSE(seenAnimating) << "A freshly constructed node should not be animating.";

	node -> decrRef();
}

/**
 * setAnimating() called from Lua without the emit argument (or with it false) flips the node's own
 * animating flag. That flag is node state rather than a Lua global, so it must still be set on the next
 * invoke via getAnimating() regardless of what the script itself does with its own globals.
 */
TEST(AnimateScriptNodeTest, SetAnimatingFromLuaPersistsAcrossInvokesWithoutEmitting)
{
	TestAnimateScriptNode* node = new TestAnimateScriptNode(
		"priorAnimating = getAnimating()\n"
		"setAnimating(true)\n"
		"afterAnimating = getAnimating()\n", "");

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());

	bool priorAnimating = true, afterAnimating = false;
	ASSERT_TRUE(target -> getGlobal("priorAnimating", priorAnimating));
	ASSERT_TRUE(target -> getGlobal("afterAnimating", afterAnimating));

	EXPECT_FALSE(priorAnimating) << "Node should not have been animating before the first setAnimating() call.";
	EXPECT_TRUE(afterAnimating) << "setAnimating(true) should be reflected immediately by getAnimating().";

	// Second invoke: the script re-reads getAnimating() into priorAnimating itself, exercising that the
	// animating flag (node state, not a Lua global) is what persisted, not the priorAnimating global.
	ASSERT_TRUE(target -> invoke());

	ASSERT_TRUE(target -> getGlobal("priorAnimating", priorAnimating));
	EXPECT_TRUE(priorAnimating) << "Animating flag set by a previous invoke should persist to later invokes.";

	node -> decrRef();
}

/**
 * getAnimateActionTarget() is the polymorphic hook AnimateAction::_apply() dispatches through; it must
 * resolve to the node itself so an AnimateAction can reach its setAnimating() override.
 */
TEST(AnimateScriptNodeTest, GetAnimateActionTargetReturnsSelf)
{
	TestAnimateScriptNode* node = new TestAnimateScriptNode("", "");

	EXPECT_EQ(node -> getAnimateActionTarget(), static_cast<AnimateActionTarget*>(node));

	node -> decrRef();
}

/**
 * A script calling setAnimating(true, true) both flips its own node's animating flag and emits an
 * AnimateAction that traverses the graph, setting the animating flag on every connected AnimateActionTarget
 * downstream. This is the mechanism a node uses to kick off animation across a subgraph from a single
 * script call.
 */
TEST(AnimateScriptNodeTest, AnimateActionTriggeredFromLuaScriptPropagatesToConnectedNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	TestAnimateScriptNode* triggerNode = new TestAnimateScriptNode(
		"priorAnimating = getAnimating()\n"
		"setAnimating(true, true)\n"
		"afterAnimating = getAnimating()\n", "");

	// downstreamNode's script only ever reads the flag; it is re-invoked explicitly below once the hive is
	// fully quiet, since the trigger's own ScriptAction traversal may otherwise reach this node before the
	// AnimateAction it spawned does.
	TestAnimateScriptNode* downstreamNode = new TestAnimateScriptNode("seenAnimating = getAnimating()", "");

	hive -> addNode(triggerNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	triggerNode -> createEdge(downstreamHandle, {});

	Handle<GraphNode> triggerHandle(triggerNode);
	ScriptAction* action = new ScriptAction(triggerHandle);
	action -> setApplyToInitialNode();

	action -> incrRef();
	action -> start();

	// Wait for the whole hive to go quiet rather than just this action: the AnimateAction it spawns is a
	// second, independently traversing action, registered active with the hive synchronously as part of
	// the setAnimating() call above.
	hive -> waitOnInitialActionActive(0);
	hive -> waitOnNoActionsActive(0);

	bool priorAnimating = true, afterAnimating = false;
	ASSERT_TRUE(triggerNode -> getScriptActionTarget() -> getGlobal("priorAnimating", priorAnimating));
	ASSERT_TRUE(triggerNode -> getScriptActionTarget() -> getGlobal("afterAnimating", afterAnimating));
	EXPECT_FALSE(priorAnimating) << "Trigger node should not have been animating before its own script ran.";
	EXPECT_TRUE(afterAnimating) << "Trigger node's own flag should flip immediately.";

	// Re-invoke downstreamNode's script fresh, now that the hive is quiet, for an authoritative read.
	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> invoke());

	bool seenAnimating = false;
	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> getGlobal("seenAnimating", seenAnimating));
	EXPECT_TRUE(seenAnimating) << "AnimateAction emitted by the trigger node's script should have propagated "
		"to the connected downstream node.";

	action -> decrRef();

	hive -> shutdown();
}

/**
 * setAnimating() called from Lua with the emit argument omitted (defaulting false) must not emit an
 * AnimateAction: a connected downstream node's animating flag should be left untouched.
 */
TEST(AnimateScriptNodeTest, SetAnimatingWithoutEmitArgumentDoesNotPropagate)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	TestAnimateScriptNode* triggerNode = new TestAnimateScriptNode("setAnimating(true)", "");
	TestAnimateScriptNode* downstreamNode = new TestAnimateScriptNode("seenAnimating = getAnimating()", "");

	hive -> addNode(triggerNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	triggerNode -> createEdge(downstreamHandle, {});

	Handle<GraphNode> triggerHandle(triggerNode);
	ScriptAction* action = new ScriptAction(triggerHandle);
	action -> setApplyToInitialNode();

	action -> incrRef();
	action -> start();

	hive -> waitOnInitialActionActive(0);
	hive -> waitOnNoActionsActive(0);

	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> invoke());

	bool seenAnimating = true;
	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> getGlobal("seenAnimating", seenAnimating));
	EXPECT_FALSE(seenAnimating) << "setAnimating() without emit=true should not emit an AnimateAction.";

	action -> decrRef();

	hive -> shutdown();
}

/**
 * A trigger script that emits two AnimateActions in sequence (animating on, then off) must leave a
 * connected downstream node in the final state: per-node action application order is preserved even though
 * the two actions traverse the graph independently.
 */
TEST(AnimateScriptNodeTest, SequentialAnimateActionsResolveToFinalState)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	TestAnimateScriptNode* triggerNode = new TestAnimateScriptNode(
		"setAnimating(true, true)\n"
		"setAnimating(false, true)\n", "");

	TestAnimateScriptNode* downstreamNode = new TestAnimateScriptNode("seenAnimating = getAnimating()", "");

	hive -> addNode(triggerNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	triggerNode -> createEdge(downstreamHandle, {});

	Handle<GraphNode> triggerHandle(triggerNode);
	ScriptAction* action = new ScriptAction(triggerHandle);
	action -> setApplyToInitialNode();

	action -> incrRef();
	action -> start();

	hive -> waitOnInitialActionActive(0);
	hive -> waitOnNoActionsActive(0);

	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> invoke());

	bool seenAnimating = true;
	ASSERT_TRUE(downstreamNode -> getScriptActionTarget() -> getGlobal("seenAnimating", seenAnimating));
	EXPECT_FALSE(seenAnimating) << "Downstream node should end up not-animating after the on/off sequence.";

	action -> decrRef();

	hive -> shutdown();
}

#endif
