#ifndef TEXT_CHANGE_ACTION_UNIT_TEST_H
#define TEXT_CHANGE_ACTION_UNIT_TEST_H

#include <gtest/gtest.h>

#include <string>

#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../util/Handle.hpp"

namespace
{
	/**
	 * Run an emitting node's core script once, then wait for every action it emitted to finish.
	 * @returns Whether the run succeeded.
	 */
	bool _textChangeRunAndWait(GraphHive* hive, ScriptNode* emittingNode)
	{
		bool ran = false;

		{ Handle<ScriptSession> sessionHandle = emittingNode -> requestCoreSession();

			ran = sessionHandle.getInstance() -> run();
		}

		hive -> waitOnNoActionsActive(0);

		return ran;
	}

	/**
	 * Emit the given emitAction() call from a node with a single edge to a text node holding initialText, in the
	 * given mode, and return the text it is left with.
	 */
	std::string _textChangeApply(const std::string& emitCall, const std::string& initialText,
		SceneTextScriptNode::TextChangeMode mode = SceneTextScriptNode::TextChangeMode::DIRECT,
		const std::string& textCoreScript = "")
	{
		GraphHive* hive = new GraphHive(2);
		Handle<GraphHive> hiveHandle(hive);

		ScriptNode* emittingNode = new ScriptNode(emitCall, "");
		SceneTextScriptNode* textNode = new SceneTextScriptNode(textCoreScript, "");

		textNode -> setText(initialText);
		textNode -> setTextChangeMode(mode);

		hive -> addNode(emittingNode);
		hive -> addNode(textNode);

		Handle<GraphNode> textHandle(textNode);
		emittingNode -> createEdge(textHandle, {});

		EXPECT_TRUE(_textChangeRunAndWait(hive, emittingNode)) << "The emitting script should have run.";

		std::string text = textNode -> getText();

		hive -> shutdown();

		return text;
	}
}

/**
 * In DIRECT mode, the default, each operation is applied at once.
 */
TEST(TextChangeActionTest, DirectModeAppliesEachOperation)
{
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "old"), "new")
		<< "REPLACE should be the default operation.";
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new', operation = 'REPLACE' })", "old"),
		"new");
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = '!', operation = 'APPEND' })", "old"),
		"old!");
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = '> ', operation = 'PREPEND' })", "old"),
		"> old");
}

/**
 * A new text node takes text changes directly.
 */
TEST(TextChangeActionTest, DirectIsTheDefaultMode)
{
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");
	Handle<GraphNode> textHandle(textNode);

	EXPECT_EQ(textNode -> getTextChangeMode(), SceneTextScriptNode::TextChangeMode::DIRECT);
}

/**
 * A LOCKED node ignores every text change.
 */
TEST(TextChangeActionTest, LockedModeIgnoresChanges)
{
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "old",
		SceneTextScriptNode::TextChangeMode::LOCKED), "old");
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = '!', operation = 'APPEND' })", "old",
		SceneTextScriptNode::TextChangeMode::LOCKED), "old");
}

/**
 * In SCRIPT mode the change is handed to onTextChange(request), which sees the operation and text and makes the
 * change itself.
 */
TEST(TextChangeActionTest, ScriptModeHandsRequestToHandler)
{
	const std::string coreScript =
		"function invoke() end\n"
		"function onTextChange(request) setText(getText() .. '|' .. request.operation .. ':' .. request.text) end\n";

	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'abc', operation = 'PREPEND' })", "old",
		SceneTextScriptNode::TextChangeMode::SCRIPT, coreScript), "old|PREPEND:abc");
}

/**
 * In SCRIPT mode nothing is applied on the handler's behalf: a handler that does nothing, or no handler at all,
 * leaves the text as it was.
 */
TEST(TextChangeActionTest, ScriptModeAppliesNothingByItself)
{
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "old",
		SceneTextScriptNode::TextChangeMode::SCRIPT, "function invoke() end\nfunction onTextChange(request) end\n"),
		"old");
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "old",
		SceneTextScriptNode::TextChangeMode::SCRIPT, "function invoke() end\n"), "old");
	EXPECT_EQ(_textChangeApply("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "old",
		SceneTextScriptNode::TextChangeMode::SCRIPT, ""), "old");
}

/**
 * A handler that raises changes nothing, does not strand the action, and leaves the node able to handle the next
 * change.
 */
TEST(TextChangeActionTest, ScriptModeHandlerThatRaisesIsContained)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("emitAction(ActionType.TEXT_CHANGE, { text = 'new' })", "");
	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"function invoke() end\n"
		"calls = 0\n"
		"function onTextChange(request)\n"
		"	calls = calls + 1\n"
		"	if calls == 1 then setText('partial') error('handler raised') end\n"
		"	setText(request.text .. calls)\n"
		"end\n", "");

	textNode -> setText("old");
	textNode -> setTextChangeMode(SceneTextScriptNode::TextChangeMode::SCRIPT);

	hive -> addNode(emittingNode);
	hive -> addNode(textNode);

	Handle<GraphNode> textHandle(textNode);
	emittingNode -> createEdge(textHandle, {});

	ASSERT_TRUE(_textChangeRunAndWait(hive, emittingNode));

	EXPECT_EQ(textNode -> getText(), "partial")
		<< "What the handler did before raising stands; nothing is applied on its behalf.";

	ASSERT_TRUE(_textChangeRunAndWait(hive, emittingNode));

	EXPECT_EQ(textNode -> getText(), "new2") << "The handler should still be called after one that raised.";

	hive -> shutdown();
}

/**
 * The node's core script can switch its own mode with setTextChangeMode() and read it back with
 * getTextChangeMode(); an unknown mode name raises.
 */
TEST(TextChangeActionTest, LuaTextChangeModeBindings)
{
	SceneTextScriptNode* textNode = new SceneTextScriptNode(
		"local before = getTextChangeMode()\n"
		"setTextChangeMode('LOCKED')\n"
		"setText(before .. ',' .. getTextChangeMode())\n", "");
	Handle<GraphNode> textHandle(textNode);

	{ Handle<ScriptSession> sessionHandle = textNode -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	EXPECT_EQ(textNode -> getText(), "DIRECT,LOCKED");
	EXPECT_EQ(textNode -> getTextChangeMode(), SceneTextScriptNode::TextChangeMode::LOCKED);

	SceneTextScriptNode* badNode = new SceneTextScriptNode("setTextChangeMode('direct')\nsetText('reached')\n", "");
	Handle<GraphNode> badHandle(badNode);

	{ Handle<ScriptSession> sessionHandle = badNode -> requestCoreSession();

		EXPECT_FALSE(sessionHandle.getInstance() -> run()) << "An unknown mode name should raise.";
	}

	EXPECT_EQ(badNode -> getText(), "") << "The script should have stopped at the bad call.";
	EXPECT_EQ(badNode -> getTextChangeMode(), SceneTextScriptNode::TextChangeMode::DIRECT);
}

/**
 * A nodeName restricts the change to nodes with that name, and the action passes over nodes holding no text to
 * reach text nodes beyond them.
 */
TEST(TextChangeActionTest, NodeNameRestrictsAndPassesThroughNonTextNodes)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode(
		"emitAction(ActionType.TEXT_CHANGE, { text = 'changed', nodeName = 'target' })", "");
	ScriptNode* middleNode = new ScriptNode("", "");
	SceneTextScriptNode* namedNode = new SceneTextScriptNode("", "");
	SceneTextScriptNode* otherNode = new SceneTextScriptNode("", "");

	namedNode -> setText("old");
	otherNode -> setText("old");

	namedNode -> setName("target");
	otherNode -> setName("bystander");

	hive -> addNode(emittingNode);
	hive -> addNode(middleNode);
	hive -> addNode(namedNode);
	hive -> addNode(otherNode);

	// A chain, as an action takes one path through a graph rather than every branch of it. The bystander comes
	// first, so the action is known to have reached it.
	Handle<GraphNode> middleHandle(middleNode);
	Handle<GraphNode> namedHandle(namedNode);
	Handle<GraphNode> otherHandle(otherNode);

	emittingNode -> createEdge(middleHandle, {});
	middleNode -> createEdge(otherHandle, {});
	otherNode -> createEdge(namedHandle, {});

	ASSERT_TRUE(_textChangeRunAndWait(hive, emittingNode));

	EXPECT_EQ(namedNode -> getText(), "changed") << "The named node beyond a non-text node should have changed.";
	EXPECT_EQ(otherNode -> getText(), "old") << "A node with another name should be left alone.";

	hive -> shutdown();
}

/**
 * The traversal keys a text change holds gate keyed edges as for any other action.
 */
TEST(TextChangeActionTest, TraversalKeysGateKeyedEdges)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("emitAction(ActionType.TEXT_CHANGE, { text = 'changed' }, { 5 })", "");
	SceneTextScriptNode* keyedNode = new SceneTextScriptNode("", "");
	SceneTextScriptNode* lockedNode = new SceneTextScriptNode("", "");

	hive -> addNode(emittingNode);
	hive -> addNode(keyedNode);
	hive -> addNode(lockedNode);

	Handle<GraphNode> keyedHandle(keyedNode);
	Handle<GraphNode> lockedHandle(lockedNode);

	emittingNode -> createEdge(keyedHandle, {}, 5);
	emittingNode -> createEdge(lockedHandle, {}, 6);

	ASSERT_TRUE(_textChangeRunAndWait(hive, emittingNode));

	EXPECT_EQ(keyedNode -> getText(), "changed") << "The node behind the held key should have changed.";
	EXPECT_EQ(lockedNode -> getText(), "") << "The node behind a key not held should not be reached.";

	hive -> shutdown();
}

/**
 * Every malformed TEXT_CHANGE call raises, stopping the script and emitting nothing.
 */
TEST(TextChangeActionTest, EmitActionRejectsMalformedCalls)
{
	const char* const badCalls[] = {
		"emitAction(ActionType.TEXT_CHANGE)",
		"emitAction(ActionType.TEXT_CHANGE, {})",
		"emitAction(ActionType.TEXT_CHANGE, { text = 7 })",
		"emitAction(ActionType.TEXT_CHANGE, { text = 'a', operation = 'append' })",
		"emitAction(ActionType.TEXT_CHANGE, { text = 'a', operation = 1 })",
		"emitAction(ActionType.TEXT_CHANGE, { text = 'a', nodeName = 1 })",
		"emitAction(ActionType.TEXT_CHANGE, { text = 'a', extra = 1 })",
		"emitAction(ActionType.TEXT_CHANGE, { text = 'a' }, { 0 })"};

	for(const char* badCall : badCalls)
	{
		GraphHive* hive = new GraphHive(2);
		Handle<GraphHive> hiveHandle(hive);

		ScriptNode* emittingNode = new ScriptNode(badCall, "");
		SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

		textNode -> setText("old");

		hive -> addNode(emittingNode);
		hive -> addNode(textNode);

		Handle<GraphNode> textHandle(textNode);
		emittingNode -> createEdge(textHandle, {});

		EXPECT_FALSE(_textChangeRunAndWait(hive, emittingNode)) << badCall << " should have raised.";
		EXPECT_EQ(textNode -> getText(), "old") << badCall << " should have emitted nothing.";

		hive -> shutdown();
	}
}

#endif
