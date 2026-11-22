#ifndef FOCUS_ACTION_UNIT_TEST_H
#define FOCUS_ACTION_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/pngImage.hpp"
#include "../../../graph/nodes/SceneGeometryNode.hpp"
#include "../../../graph/nodes/SceneImageNode.hpp"
#include "../../../graph/nodes/SceneRootNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../util/Handle.hpp"

namespace
{
	/**
	 * Run a scene action from the given root into the given surface.
	 */
	void _focusTestCollect(SceneRootNode* root, GraphHiveSceneSurface* surface)
	{
		Handle<GraphNode> rootHandle(root);
		SceneAction* sceneAction = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(surface));

		sceneAction -> incrRef();
		sceneAction -> start();
		sceneAction -> waitOnComplete(0);
		sceneAction -> decrRef();
	}

	/**
	 * Run a script node's core script and wait for every action it emits to finish.
	 */
	void _focusTestRun(GraphHive* hive, ScriptNode* node)
	{
		{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

			ASSERT_TRUE(sessionHandle.getInstance() -> run());
		}

		hive -> waitOnNoActionsActive(0);
	}
}

/**
 * A script's FOCUS action asks every geometry, text and image node it reaches to be focused on, or only those
 * with the name it is given.
 */
TEST(FocusActionTest, FocusActionReachesEveryVisualNodeType)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* focusAll = new ScriptNode("emitAction(ActionType.FOCUS)", "");
	ScriptNode* focusNamed = new ScriptNode("emitAction(ActionType.FOCUS, { nodeName = 'picture' })", "");
	ScriptNode* middleNode = new ScriptNode("", "");
	SceneGeometryNode* geometryNode = new SceneGeometryNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");
	SceneImageNode* imageNode = new SceneImageNode();

	imageNode -> setName("picture");

	hive -> addNode(focusAll);
	hive -> addNode(focusNamed);
	hive -> addNode(middleNode);
	hive -> addNode(geometryNode);
	hive -> addNode(textNode);
	hive -> addNode(imageNode);

	// A chain, as an action takes one path through a graph rather than every branch of it.
	Handle<GraphNode> middleHandle(middleNode);
	Handle<GraphNode> geometryHandle(geometryNode);
	Handle<GraphNode> textHandle(textNode);
	Handle<GraphNode> imageHandle(imageNode);

	focusAll -> createEdge(middleHandle, {});
	focusNamed -> createEdge(middleHandle, {});
	middleNode -> createEdge(geometryHandle, {});
	geometryNode -> createEdge(textHandle, {});
	textNode -> createEdge(imageHandle, {});

	_focusTestRun(hive, focusAll);

	EXPECT_EQ(geometryNode -> getFocusRequestCount(), 1u);
	EXPECT_EQ(textNode -> getFocusRequestCount(), 1u);
	EXPECT_EQ(imageNode -> getFocusRequestCount(), 1u);

	_focusTestRun(hive, focusNamed);

	EXPECT_EQ(geometryNode -> getFocusRequestCount(), 1u) << "A named FOCUS should pass other nodes by.";
	EXPECT_EQ(textNode -> getFocusRequestCount(), 1u) << "A named FOCUS should pass other nodes by.";
	EXPECT_EQ(imageNode -> getFocusRequestCount(), 2u);

	hive -> shutdown();
}

/**
 * A focus request reaches a surface on its next populate, which it forces on its own. A request made before the
 * surface first populated is taken as already seen, and a populate with no new request leaves the last one be.
 */
TEST(FocusActionTest, SurfacePassesOnNewFocusRequests)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneImageNode* imageNode = new SceneImageNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

	imageNode -> setSource(pngImage::fromUrl("https://example.org/a.webp"));

	hive -> addNode(root);
	hive -> addNode(imageNode);
	hive -> addNode(textNode);

	Handle<GraphNode> imageHandle(imageNode);
	Handle<GraphNode> textHandle(textNode);

	root -> createEdge(imageHandle, {});
	imageNode -> createEdge(textHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	unsigned sequence = 99;
	unsigned nodeId = 99;
	double fraction = 99;

	imageNode -> requestFocus();

	_focusTestCollect(root, surface);
	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_EQ(sequence, 0u) << "A request from before the first populate should not be passed on.";

	unsigned populateVersion = surface -> getPopulateVersion();

	textNode -> requestFocus();

	_focusTestCollect(root, surface);
	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_NE(surface -> getPopulateVersion(), populateVersion) << "A focus request alone should force a populate.";
	EXPECT_EQ(sequence, 1u);
	EXPECT_EQ(nodeId, textNode -> getId());
	EXPECT_EQ(fraction, 0) << "A request with no viewport fraction should ask for right-click behaviour.";

	_focusTestCollect(root, surface);
	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_EQ(sequence, 1u) << "A populate with no new request should not pass one on.";

	imageNode -> requestFocus();

	_focusTestCollect(root, surface);
	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_EQ(sequence, 2u);
	EXPECT_EQ(nodeId, imageNode -> getId());

	// Asked again for the same node, a viewer must still be able to tell it is a new request, and how it asks to
	// be focused on this time.
	imageNode -> requestFocus(0.25);

	_focusTestCollect(root, surface);
	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_EQ(sequence, 3u);
	EXPECT_EQ(nodeId, imageNode -> getId());
	EXPECT_DOUBLE_EQ(fraction, 0.25);

	surface -> close();

	hive -> shutdown();
}

/**
 * A script's viewportFraction travels with its FOCUS request through to the surface.
 */
TEST(FocusActionTest, ScriptViewportFractionReachesSurface)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneImageNode* imageNode = new SceneImageNode();
	ScriptNode* focusFit = new ScriptNode("emitAction(ActionType.FOCUS, { viewportFraction = 0.3 })", "");

	imageNode -> setSource(pngImage::fromUrl("https://example.org/a.webp"));

	hive -> addNode(root);
	hive -> addNode(imageNode);
	hive -> addNode(focusFit);

	Handle<GraphNode> imageHandle(imageNode);

	root -> createEdge(imageHandle, {});
	focusFit -> createEdge(imageHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	_focusTestCollect(root, surface);

	_focusTestRun(hive, focusFit);

	_focusTestCollect(root, surface);

	unsigned sequence = 0;
	unsigned nodeId = 0;
	double fraction = 0;

	surface -> getFocusRequest(sequence, nodeId, fraction);

	EXPECT_EQ(sequence, 1u);
	EXPECT_EQ(nodeId, imageNode -> getId());
	EXPECT_DOUBLE_EQ(fraction, 0.3);

	surface -> close();

	hive -> shutdown();
}

/**
 * A malformed FOCUS call raises a Lua error.
 */
TEST(FocusActionTest, MalformedFocusCallsRaise)
{
	const char* const badCalls[] = {
		"emitAction(ActionType.FOCUS, 1)",
		"emitAction(ActionType.FOCUS, { nodeName = 1 })",
		"emitAction(ActionType.FOCUS, { nodeName = 'a', extra = 1 })",
		"emitAction(ActionType.FOCUS, { viewportFraction = 0 })",
		"emitAction(ActionType.FOCUS, { viewportFraction = 1.5 })",
		"emitAction(ActionType.FOCUS, { viewportFraction = 0/0 })",
		"emitAction(ActionType.FOCUS, { viewportFraction = '0.5' })"};

	for(const char* call : badCalls)
	{
		GraphHive* hive = new GraphHive(2);
		Handle<GraphHive> hiveHandle(hive);

		ScriptNode* node = new ScriptNode(call, "");

		hive -> addNode(node);

		{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

			EXPECT_FALSE(sessionHandle.getInstance() -> run()) << call;
		}

		hive -> shutdown();
	}
}

#endif
