#ifndef AGENT_ACTION_PROGRESS_UNIT_TEST_H
#define AGENT_ACTION_PROGRESS_UNIT_TEST_H

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../../agent/AgenticHarness.hpp"
#include "../../../agent/Model.hpp"
#include "../../../agent/ModelContext.hpp"
#include "../../../agent/ModelProvider.hpp"
#include "../../../agent/ModelRequest.hpp"
#include "../../../agent/ModelRequestProgressListener.hpp"
#include "../../../graph/actions/AgentAction.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/nodes/AgentNode.hpp"
#include "../../../graph/nodes/SceneGeometryNode.hpp"
#include "../../../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../util/Handle.hpp"

namespace
{
	/// Progress the fake provider reports for its first round of tool calls.
	const std::string FIRST_ROUND_PROGRESS = "Looking at the node.\nCalled tool getColour.";

	/// Progress the fake provider reports for its second round of tool calls.
	const std::string SECOND_ROUND_PROGRESS = "Called tool setColour.\nCalled tool setScale.";

	/**
	 * Provider that services every request without going near a server, making two rounds of tool calls on the
	 * way to a fixed response.
	 */
	class ProgressTestModelProvider : public ModelProvider
	{
		public:

			ProgressTestModelProvider() {}

		protected:

			virtual ~ProgressTestModelProvider() {}

			virtual void _populateModels() override {}

			virtual std::string _processRequest(Handle<Model> model, ModelRequest& request,
				std::vector<ModelContext::ToolCallRound>& toolCallRounds) override
			{
				ModelContext::ToolCallRound first;

				first.content = "Looking at the node.";
				first.toolCalls.push_back(ModelContext::ToolCall{"", "getColour", "", "{}"});

				toolCallRounds.push_back(first);
				_reportToolCallRound(request, first);

				ModelContext::ToolCallRound second;

				second.toolCalls.push_back(ModelContext::ToolCall{"", "setColour", "", "{}"});
				second.toolCalls.push_back(ModelContext::ToolCall{"", "setScale", "", "{}"});

				toolCallRounds.push_back(second);
				_reportToolCallRound(request, second);

				return "Done.";
			}
	};

	/**
	 * Model sourced from a ProgressTestModelProvider.
	 */
	class ProgressTestModel : public Model
	{
		public:

			ProgressTestModel(Handle<ModelProvider> provider) : Model(provider, "progressTest", "", "0", "0", true) {}

			virtual std::string processRequest(ModelRequest& request) override
			{
				return _processRequest(request);
			}

		protected:

			virtual ~ProgressTestModel() {}
	};

	/**
	 * Records every progress report it is given.
	 */
	class RecordingProgressListener : public ModelRequestProgressListener
	{
		public:

			virtual void agentRequestProgressReport(std::string text) override
			{
				reports.push_back(text);
			}

			/// Reports received, oldest first.
			std::vector<std::string> reports;
	};

	/**
	 * SceneGeometryNode that records the progress passed on to it by an agent action.
	 */
	class ProgressRecordingGeometryNode : public SceneGeometryNode
	{
		public:

			ProgressRecordingGeometryNode()
			{
				// A plain SceneGeometryNode is only ever affected by agent actions applied elsewhere.
				_addActionFlag(AGENT_GRAPH_ACTION);
			}

			virtual void agentProgress(std::string text) override
			{
				// Only written on the thread applying the action, and only read once the action is complete.
				reports.push_back(text);
			}

			virtual void agentPrompt(std::string prompt) override
			{
				prompts.push_back(prompt);
			}

			virtual void agentResponse(std::string response) override
			{
				responses.push_back(response);
			}

			/// Reports received, oldest first.
			std::vector<std::string> reports;

			/// Prompts received, oldest first.
			std::vector<std::string> prompts;

			/// Responses received, oldest first.
			std::vector<std::string> responses;
	};

	/**
	 * Build a harness whose node role at HIGH capability is serviced by a ProgressTestModel.
	 */
	Handle<AgenticHarness> _buildProgressTestHarness()
	{
		Handle<ModelProvider> provider(new ProgressTestModelProvider());
		Handle<Model> model(new ProgressTestModel(provider));

		Handle<AgenticHarness> harness(new AgenticHarness());

		harness.getInstance() -> addModelAssignment(
			AgenticHarness::RoleCapability{AgenticHarness::Role::NODE, AgenticHarness::Capability::HIGH}, model);

		return harness;
	}
}

/**
 * Each round of tool calls made while servicing a request is reported to the request's listener as it is made,
 * worded the same way whichever provider made it.
 */
TEST(AgentActionProgressTest, HarnessRequest_ReportsEachToolCallRound)
{
	Handle<AgenticHarness> harness = _buildProgressTestHarness();

	RecordingProgressListener listener;

	harness.getInstance() -> processRequest("Do something.", AgenticHarness::Role::NODE,
		AgenticHarness::Capability::HIGH, Handle<ModelContext>(0), &listener);

	ASSERT_EQ(listener.reports.size(), 2u);
	EXPECT_EQ(listener.reports[0], FIRST_ROUND_PROGRESS);
	EXPECT_EQ(listener.reports[1], SECOND_ROUND_PROGRESS);
}

/**
 * A request made without a listener is serviced as normal, its tool call rounds still recorded in the context.
 */
TEST(AgentActionProgressTest, HarnessRequestWithoutListener_StillServiced)
{
	Handle<AgenticHarness> harness = _buildProgressTestHarness();

	Handle<ModelContext> context = harness.getInstance() -> processRequest("Do something.",
		AgenticHarness::Role::NODE, AgenticHarness::Capability::HIGH);

	ASSERT_TRUE(context.isValid());
	EXPECT_EQ(context.getInstance() -> getLastResponse(), "Done.");

	std::vector<ModelContext::ChatExchange> history = context.getInstance() -> getChatHistory();

	ASSERT_EQ(history.size(), 1u);
	EXPECT_EQ(history[0].toolCallRounds.size(), 2u);
}

/**
 * An agent action keeps the progress reported by the request it makes for a node, and passes each report on to
 * that node as it arrives.
 */
TEST(AgentActionProgressTest, AgentAction_KeepsProgressAndPassesItToNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	hive -> setAgenticHarness(_buildProgressTestHarness());

	AgentAction::NodePrompt prompt;

	prompt.nodeType = GraphNodeType::SCENE_GEOMETRY_NODE;
	prompt.prompt = "Do something.";
	prompt.terminateOnResponse = true;

	AgentNode* agentNode = new AgentNode(AgenticHarness::Capability::HIGH, {prompt}, false);
	ProgressRecordingGeometryNode* geometryNode = new ProgressRecordingGeometryNode();

	hive -> addNode(agentNode);
	hive -> addNode(geometryNode);

	Handle<GraphNode> geometryHandle(geometryNode);
	agentNode -> createEdge(geometryHandle, {AGENT_GRAPH_ACTION});

	AgentAction* action = agentNode -> emitAgentAction(true);

	std::vector<std::string> progress = action -> getProgress();

	ASSERT_EQ(progress.size(), 2u);
	EXPECT_EQ(progress[0], FIRST_ROUND_PROGRESS);
	EXPECT_EQ(progress[1], SECOND_ROUND_PROGRESS);

	EXPECT_EQ(geometryNode -> reports, progress) << "The node the request was made for should have been given "
		"each report the action received.";

	EXPECT_EQ(geometryNode -> prompts, std::vector<std::string>{"Do something."});
	EXPECT_EQ(geometryNode -> responses, std::vector<std::string>{"Done."});

	action -> decrRef();

	hive -> shutdown();
}

TEST(AgentActionProgressTest, AgentAction_HandsConversationToScriptHandlers)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	hive -> setAgenticHarness(_buildProgressTestHarness());

	AgentAction::NodePrompt prompt;

	prompt.nodeType = GraphNodeType::SCENE_GEOMETRY_SCRIPT_NODE;
	prompt.prompt = "Do something.";
	prompt.terminateOnResponse = true;

	// The handlers only hand the conversation on once the request has ended, so the text node is changed once.
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"local log = ''\n"
		"function init() end\n"
		"function onAgentPrompt(p) log = 'P:' .. p end\n"
		"function onAgentProgress(t) log = log .. '|' .. t end\n"
		"function onAgentResponse(r) log = log .. '|R:' .. r end\n"
		"function onAgentEnd() emitAction(ActionType.TEXT_CHANGE, {text = log .. '|E'}, {7}) end\n", "");

	AgentNode* agentNode = new AgentNode(AgenticHarness::Capability::HIGH, {prompt}, false);
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

	hive -> addNode(agentNode);
	hive -> addNode(geometryNode);
	hive -> addNode(textNode);

	Handle<GraphNode> geometryHandle(geometryNode);
	Handle<GraphNode> textHandle(textNode);

	agentNode -> createEdge(geometryHandle, {AGENT_GRAPH_ACTION});
	geometryNode -> createEdge(textHandle, {TEXT_CHANGE_GRAPH_ACTION}, 7);

	// Handlers are only called once a run of the script has defined them.
	{ Handle<ScriptSession> sessionHandle = geometryNode -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	AgentAction* action = agentNode -> emitAgentAction(true);

	action -> decrRef();

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(textNode -> getText(), "P:Do something.|" + FIRST_ROUND_PROGRESS + "|" + SECOND_ROUND_PROGRESS
		+ "|R:Done.|E");

	hive -> shutdown();
}

#endif
