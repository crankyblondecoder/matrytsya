#ifndef AGENT_VISIBLE_ANIMATE_UNIT_TEST_H
#define AGENT_VISIBLE_ANIMATE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/actions/StrobeAction.hpp"
#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../../../graph/nodes/SceneRootNode.hpp"
#include "../../../graph/nodes/SceneTransformScriptNode.hpp"

namespace
{
	// Mirrors flower 2 in examples/flowerHive.json: a geometry body whose wildcard edge leads to a single
	// transform script node that spins only while it is in animating mode.
	struct SpinRig
	{
		GraphHive* hive;
		SceneRootNode* root;
		SceneGeometryScriptNode* body;
		SceneTransformScriptNode* spin;
		GraphHiveSceneSurface* surface;
	};

	SpinRig buildSpinRig()
	{
		SpinRig rig;

		rig.hive = new GraphHive(4);

		Handle<GraphHive> hiveHandle(rig.hive);

		rig.root = new SceneRootNode();
		rig.body = new SceneGeometryScriptNode("if vertexCount() == 0 then addVertex(Vertex{posn = {1, 2, 3}}) end", "");
		rig.spin = new SceneTransformScriptNode(
			"if getStrobe() and getAnimating() then local t = getTransform() t[13] = t[13] + 1 setTransform(t) end", "");

		rig.hive -> addNode(rig.root);
		rig.hive -> addNode(rig.body);
		rig.hive -> addNode(rig.spin);

		Handle<GraphNode> bodyHandle(rig.body);
		Handle<GraphNode> spinHandle(rig.spin);

		rig.root -> createEdge(bodyHandle, {});
		rig.body -> createEdge(spinHandle, {});

		rig.surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(rig.root));
		rig.surface -> setHive(hiveHandle);

		return rig;
	}

	void strobeRig(SpinRig& rig)
	{
		Handle<GraphNode> rootHandle(rig.root);

		StrobeAction* action = new StrobeAction(rootHandle);

		action -> incrRef();
		action -> start();
		action -> waitOnComplete(0);
		action -> decrRef();
	}

	// Returns the translation X the surface currently holds for the spin node's transform.
	double populateAndReadSpin(SpinRig& rig)
	{
		Handle<GraphNode> rootHandle(rig.root);

		SceneAction* action = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(rig.surface));

		action -> incrRef();
		action -> start();
		action -> waitOnComplete(0);
		action -> decrRef();

		GraphHiveSceneSurface::Scene scene = rig.surface -> getScene();

		for(const GraphHiveSceneSurface::ModelTransform& transform : scene.modelTransforms)
		{
			if(transform.id == rig.spin -> getId()) return transform.transform[12];
		}

		return -1;
	}
}

// Control: the tool call path with no agent visible flag involved at all.
TEST(AgentVisibleAnimateTest, SpinAnimatesWithoutAgentVisible)
{
	SpinRig rig = buildSpinRig();

	strobeRig(rig);

	rig.body -> setAnimating(true, 1234, true);

	// The animate action is emitted asynchronously, so let the hive settle before strobing again.
	rig.hive -> waitOnNoActionsActive(0);

	EXPECT_TRUE(rig.spin -> getAnimating()) << "The animate action should have reached the spin node.";

	double first = populateAndReadSpin(rig);

	strobeRig(rig);

	double second = populateAndReadSpin(rig);

	EXPECT_NE(first, second) << "The spin node's transform should advance once it is animating.";

	rig.surface -> close();
	rig.hive -> shutdown();
}

// The same thing, but with the agent visible flag set and cleared around the tool call the way
// AgentAction::_apply now does it.
TEST(AgentVisibleAnimateTest, SpinAnimatesWithAgentVisibleAroundTheToolCall)
{
	SpinRig rig = buildSpinRig();

	strobeRig(rig);

	rig.body -> setAgentVisible(true);

	rig.body -> setAnimating(true, 1234, true);

	rig.hive -> waitOnNoActionsActive(0);

	rig.body -> setAgentVisible(false);

	EXPECT_TRUE(rig.spin -> getAnimating()) << "The animate action should have reached the spin node.";

	double first = populateAndReadSpin(rig);

	strobeRig(rig);

	double second = populateAndReadSpin(rig);

	EXPECT_NE(first, second) << "The spin node's transform should advance once it is animating.";

	rig.surface -> close();
	rig.hive -> shutdown();
}

// A transform script that runs its animation down and then clears its own animating mode leaves the node
// that started it still marked as animating. Re-asserting the mode from that node must still reach the
// transform, or nothing could ever restart it. This is what the agent's setAnimating tool does on its second
// and later visits to the same node.
TEST(AgentVisibleAnimateTest, ReassertingAnimatingRestartsANodeThatClearedItself)
{
	SpinRig rig = buildSpinRig();

	strobeRig(rig);

	rig.body -> setAnimating(true, 1, true);
	rig.hive -> waitOnNoActionsActive(0);

	ASSERT_TRUE(rig.spin -> getAnimating()) << "The first assertion should reach the spin node.";

	// Stand in for the spin script running itself down: the transform clears its own mode without emitting,
	// so the body it was started from is left still marked as animating.
	rig.spin -> setAnimating(false, 0, false);

	ASSERT_FALSE(rig.spin -> getAnimating());
	ASSERT_TRUE(rig.body -> getAnimating()) << "The body should still be marked as animating.";

	// The same call the tool makes on a later visit. The body's own flag is unchanged by it.
	rig.body -> setAnimating(true, 2, true);
	rig.hive -> waitOnNoActionsActive(0);

	EXPECT_TRUE(rig.spin -> getAnimating())
		<< "Re-asserting animating mode must emit an animate action even though the emitting node's own flag "
		   "was already set, otherwise a node that cleared itself can never be restarted.";

	rig.surface -> close();
	rig.hive -> shutdown();
}

#endif
