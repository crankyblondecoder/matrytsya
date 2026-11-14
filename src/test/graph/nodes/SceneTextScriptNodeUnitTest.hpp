#ifndef SCENE_TEXT_SCRIPT_NODE_UNIT_TEST_H
#define SCENE_TEXT_SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/actions/StrobeAction.hpp"
#include "../../../util/Handle.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/nodes/SceneRootNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/SceneTransformNode.hpp"

#include <cstddef>
#include <string>

namespace
{
	/**
	 * Run a strobe from the given root, which runs the core script of every scripted node it reaches.
	 * @param root Root to strobe from.
	 */
	void strobeTextScene(SceneRootNode* root)
	{
		Handle<GraphNode> rootHandle(root);
		StrobeAction* strobeAction = new StrobeAction(rootHandle);

		strobeAction -> incrRef();
		strobeAction -> start();
		strobeAction -> waitOnComplete(0);
		strobeAction -> decrRef();
	}

	/**
	 * Run a scene action from the given root into the given surface.
	 * @param root Root to collect the scene from.
	 * @param surface Surface to populate.
	 */
	void collectTextScene(SceneRootNode* root, GraphHiveSceneSurface* surface)
	{
		Handle<GraphNode> rootHandle(root);
		SceneAction* sceneAction = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(surface));

		sceneAction -> incrRef();
		sceneAction -> start();
		sceneAction -> waitOnComplete(0);
		sceneAction -> decrRef();
	}
}

/**
 * Text set by a script, along with where it is anchored, how tall it is and its colour, reaches the scene a
 * scene action collects into a surface.
 */
TEST(SceneTextScriptNodeTest, ScriptSetTextReachesScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setText('Hello\\nworld')\n"
		"setTextPosition({1, 2, 3})\n"
		"setTextHeight(0.5)\n"
		"setTextColour({10, 20, 30, 40})\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	collectTextScene(root, surface);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();

	ASSERT_EQ(scene.texts.size(), 1u);

	const GraphHiveSceneSurface::Text& text = scene.texts[0];

	EXPECT_EQ(text.nodeId, textNode -> getId());
	EXPECT_EQ(text.text, "Hello\nworld");
	EXPECT_DOUBLE_EQ(text.position[0], 1);
	EXPECT_DOUBLE_EQ(text.position[1], 2);
	EXPECT_DOUBLE_EQ(text.position[2], 3);
	EXPECT_DOUBLE_EQ(text.height, 0.5);
	EXPECT_EQ(std::to_integer<int>(text.colour[0]), 10);
	EXPECT_EQ(std::to_integer<int>(text.colour[1]), 20);
	EXPECT_EQ(std::to_integer<int>(text.colour[2]), 30);
	EXPECT_EQ(std::to_integer<int>(text.colour[3]), 40);
	EXPECT_EQ(text.modelTransformIndex, 0u);

	surface -> close();

	hive -> shutdown();
}

/**
 * Text collected beneath a transform is placed by that transform rather than the identity.
 */
TEST(SceneTextScriptNodeTest, TextUnderTransformUsesThatTransform)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTransformNode* transformNode = new SceneTransformNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("setText('placed')", "");

	Transform translation = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};

	transformNode -> setTransform(translation);

	hive -> addNode(root);
	hive -> addNode(transformNode);
	hive -> addNode(textNode);

	Handle<GraphNode> transformHandle(transformNode);
	Handle<GraphNode> textHandle(textNode);

	root -> createEdge(transformHandle, {});
	transformNode -> createEdge(textHandle, {});

	strobeTextScene(root);

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	collectTextScene(root, surface);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();

	ASSERT_EQ(scene.texts.size(), 1u);

	unsigned transformIndex = scene.texts[0].modelTransformIndex;

	ASSERT_LT(transformIndex, scene.modelTransforms.size());
	EXPECT_NE(transformIndex, 0u);
	EXPECT_DOUBLE_EQ(scene.modelTransforms[transformIndex].transform[12], 5);
	EXPECT_DOUBLE_EQ(scene.modelTransforms[transformIndex].transform[13], 6);
	EXPECT_DOUBLE_EQ(scene.modelTransforms[transformIndex].transform[14], 7);

	surface -> close();

	hive -> shutdown();
}

/**
 * Only an actual change to the text is versioned, so that a script re-asserting the same text every strobe does
 * not make the scene look changed.
 */
TEST(SceneTextScriptNodeTest, OnlyAnActualChangeBumpsTheVersion)
{
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");
	Handle<GraphNode> textHandle(textNode);

	textNode -> setText("same");

	unsigned version = textNode -> getVersion();

	textNode -> setText("same");
	EXPECT_EQ(textNode -> getVersion(), version);

	double position[3] = {0, 0, 0};
	textNode -> setPosition(position);
	EXPECT_EQ(textNode -> getVersion(), version);

	textNode -> setText("different");
	EXPECT_GT(textNode -> getVersion(), version);

	version = textNode -> getVersion();

	EXPECT_TRUE(textNode -> setHeight(2.0));
	EXPECT_GT(textNode -> getVersion(), version);
}

/**
 * Changing the text after it has been collected makes the next scene action carry the new text.
 */
TEST(SceneTextScriptNodeTest, ChangedTextRepopulatesScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

	textNode -> setText("before");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	collectTextScene(root, surface);

	ASSERT_EQ(surface -> getScene().texts.size(), 1u);
	EXPECT_EQ(surface -> getScene().texts[0].text, "before");

	textNode -> setText("after");

	collectTextScene(root, surface);

	ASSERT_EQ(surface -> getScene().texts.size(), 1u);
	EXPECT_EQ(surface -> getScene().texts[0].text, "after");

	surface -> close();

	hive -> shutdown();
}

/**
 * The text getters hand back to a script what the setters were given.
 */
TEST(SceneTextScriptNodeTest, GettersRoundTripThroughLua)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	// Each getter is read back and, where it matches, the next value is set from it, so the final text only
	// reads "ok" if every round trip worked.
	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setText('first')\n"
		"setTextPosition({1.5, -2, 3})\n"
		"setTextHeight(0.25)\n"
		"setTextColour({1, 2, 3, 4})\n"
		"local p = getTextPosition()\n"
		"local c = getTextColour()\n"
		"local ok = getText() == 'first' and p[1] == 1.5 and p[2] == -2 and p[3] == 3 and\n"
		"	getTextHeight() == 0.25 and c[1] == 1 and c[2] == 2 and c[3] == 3 and c[4] == 4\n"
		"setText(ok and 'ok' or 'mismatch')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_EQ(textNode -> getText(), "ok");

	hive -> shutdown();
}

/**
 * A text height that is not greater than zero is refused by the script binding, leaving the height as it was.
 */
TEST(SceneTextScriptNodeTest, NonPositiveHeightRaisesLuaError)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	// The error aborts the script, so the text after it is never set.
	SceneTextScriptNode* textNode = new SceneTextScriptNode("setTextHeight(0)\nsetText('reached')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_DOUBLE_EQ(textNode -> getHeight(), 1.0);
	EXPECT_EQ(textNode -> getText(), "");

	hive -> shutdown();
}

/**
 * A text colour channel outside 0-255 is refused by the script binding, leaving the colour as it was.
 */
TEST(SceneTextScriptNodeTest, OutOfRangeColourRaisesLuaError)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("setTextColour({256, 0, 0, 255})\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	std::byte colour[4];

	textNode -> getColour(colour);

	EXPECT_EQ(std::to_integer<int>(colour[0]), 255);
	EXPECT_EQ(std::to_integer<int>(colour[1]), 255);

	hive -> shutdown();
}

/**
 * Hidden text is left out of the scene, and showing it again puts it back.
 */
TEST(SceneTextScriptNodeTest, HiddenTextIsLeftOutOfScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

	textNode -> setText("peekaboo");
	textNode -> setVisible(false);

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	collectTextScene(root, surface);

	EXPECT_TRUE(surface -> getScene().texts.empty()) << "Hidden text should not reach the scene.";

	textNode -> setVisible(true);

	collectTextScene(root, surface);

	ASSERT_EQ(surface -> getScene().texts.size(), 1u) << "Shown text should reach the scene again.";
	EXPECT_EQ(surface -> getScene().texts[0].text, "peekaboo");

	surface -> close();

	hive -> shutdown();
}

/**
 * Only an actual change of visibility is versioned.
 */
TEST(SceneTextScriptNodeTest, OnlyAnActualVisibilityChangeBumpsTheVersion)
{
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");
	Handle<GraphNode> textHandle(textNode);

	EXPECT_TRUE(textNode -> getVisible()) << "Text should be shown by default.";

	unsigned version = textNode -> getVersion();

	textNode -> setVisible(true);
	EXPECT_EQ(textNode -> getVersion(), version);

	textNode -> setVisible(false);
	EXPECT_GT(textNode -> getVersion(), version);
	EXPECT_FALSE(textNode -> getVisible());
}

/**
 * A script can hide its own text and read back whether it is shown.
 */
TEST(SceneTextScriptNodeTest, ScriptSetsAndGetsVisibility)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"local before = getTextVisible()\n"
		"setTextVisible(false)\n"
		"setText((before == true and getTextVisible() == false) and 'ok' or 'mismatch')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_EQ(textNode -> getText(), "ok");
	EXPECT_FALSE(textNode -> getVisible());

	hive -> shutdown();
}

/**
 * Only a boolean is accepted as a visibility, so a truthy number is not mistaken for one.
 */
TEST(SceneTextScriptNodeTest, NonBooleanVisibilityRaisesLuaError)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	// The error aborts the script, so the text after it is never set.
	SceneTextScriptNode* textNode = new SceneTextScriptNode("setTextVisible(0)\nsetText('reached')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_TRUE(textNode -> getVisible());
	EXPECT_EQ(textNode -> getText(), "");

	hive -> shutdown();
}

/**
 * A background set by a script, with a radius per corner, reaches the scene a scene action collects into a
 * surface.
 */
TEST(SceneTextScriptNodeTest, ScriptSetBackgroundReachesScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setText('boxed')\n"
		"setTextBackground({fillColour = {1, 2, 3, 4}, borderColour = {5, 6, 7, 8}, borderWidth = 0.05,\n"
		"	padding = 0.2, cornerRadius = {0.1, 0.2, 0.3, 0.4}})\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	collectTextScene(root, surface);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();

	ASSERT_EQ(scene.texts.size(), 1u);

	const TextBackground& background = scene.texts[0].background;

	EXPECT_EQ(std::to_integer<int>(background.fillColour[0]), 1);
	EXPECT_EQ(std::to_integer<int>(background.fillColour[3]), 4);
	EXPECT_EQ(std::to_integer<int>(background.borderColour[0]), 5);
	EXPECT_EQ(std::to_integer<int>(background.borderColour[3]), 8);
	EXPECT_DOUBLE_EQ(background.borderWidth, 0.05);
	EXPECT_DOUBLE_EQ(background.padding, 0.2);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::TOP_LEFT], 0.1);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::TOP_RIGHT], 0.2);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::BOTTOM_RIGHT], 0.3);
	EXPECT_DOUBLE_EQ(background.cornerRadii[TextBackground::BOTTOM_LEFT], 0.4);

	surface -> close();

	hive -> shutdown();
}

/**
 * Fields left out of setTextBackground() keep their value, a single cornerRadius rounds every corner, and
 * getTextBackground() reads it all back.
 */
TEST(SceneTextScriptNodeTest, BackgroundPartialSetAndGetRoundTripThroughLua)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setTextBackground({fillColour = {10, 20, 30, 255}, padding = 0.5})\n"
		"setTextBackground({cornerRadius = 0.25, borderWidth = 0.1})\n"
		"local b = getTextBackground()\n"
		"local r = b.cornerRadius\n"
		"local ok = b.fillColour[1] == 10 and b.fillColour[4] == 255 and b.borderColour[4] == 0 and\n"
		"	b.padding == 0.5 and b.borderWidth == 0.1 and\n"
		"	r[1] == 0.25 and r[2] == 0.25 and r[3] == 0.25 and r[4] == 0.25\n"
		"setText(ok and 'ok' or 'mismatch')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_EQ(textNode -> getText(), "ok");

	hive -> shutdown();
}

/**
 * A negative background length is refused by the script binding, leaving the background as it was.
 */
TEST(SceneTextScriptNodeTest, NegativeBackgroundLengthRaisesLuaError)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	// The error aborts the script, so the text after it is never set.
	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setTextBackground({fillColour = {1, 1, 1, 1}, cornerRadius = {0, 0, -1, 0}})\nsetText('reached')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_TRUE(textNode -> getBackground() == TextBackground());
	EXPECT_EQ(textNode -> getText(), "");

	hive -> shutdown();
}

/**
 * A background colour channel outside 0-255 is refused by the script binding.
 */
TEST(SceneTextScriptNodeTest, OutOfRangeBackgroundColourRaisesLuaError)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"setTextBackground({borderColour = {0, 0, 0, 256}})\nsetText('reached')\n", "");

	hive -> addNode(root);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	root -> createEdge(textHandle, {});

	strobeTextScene(root);

	EXPECT_TRUE(textNode -> getBackground() == TextBackground());
	EXPECT_EQ(textNode -> getText(), "");

	hive -> shutdown();
}

/**
 * Setting the background to what it already is does not make the scene look changed.
 */
TEST(SceneTextScriptNodeTest, OnlyAnActualBackgroundChangeBumpsTheVersion)
{
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");
	Handle<GraphNode> textHandle(textNode);

	unsigned version = textNode -> getVersion();

	EXPECT_TRUE(textNode -> setBackground(TextBackground()));
	EXPECT_EQ(textNode -> getVersion(), version);

	TextBackground background;
	background.cornerRadii[TextBackground::BOTTOM_LEFT] = 0.5;

	EXPECT_TRUE(textNode -> setBackground(background));
	EXPECT_GT(textNode -> getVersion(), version);
}

/**
 * The new node type has a name that round-trips, so it can be named by a trigger, an agent prompt or a script.
 */
TEST(SceneTextScriptNodeTest, NodeTypeNameRoundTrips)
{
	EXPECT_EQ(GraphNode::typeFromName("SCENE_TEXT_SCRIPT_NODE"), GraphNodeType::SCENE_TEXT_SCRIPT_NODE);
}

#endif
