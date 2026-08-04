#ifndef TRIGGER_NODE_UNIT_TEST_H
#define TRIGGER_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include <atomic>

#include "../../../graph/actionTargets/TriggerActionTarget.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphPoke.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../graph/nodes/TriggerNode.hpp"
#include "../../../util/Handle.hpp"

/**
 * Trigger target that counts what reaches it, so a test can tell a trigger a TriggerNode emitted itself
 * apart from one its poke script emitted.
 */
class PokeTriggerCountingNode : public ScriptNode, public TriggerActionTarget
{
	public:

		PokeTriggerCountingNode() : ScriptNode("", "")
		{
			_addActionFlag(TRIGGER_GRAPH_ACTION);
		}

		void trigger() override { triggerCount++; }

		TriggerActionTarget* getTriggerActionTarget() override { return this; }

		/// Written from whichever worker thread applies the action, read from the test thread.
		std::atomic<int> triggerCount{0};
};

namespace
{
	/**
	 * Build a hive holding a TriggerNode wired to a single downstream trigger target, with poking enabled
	 * on the TriggerNode.
	 * @param emitTriggerOnPoke Passed straight to the TriggerNode's constructor.
	 * @param pokeScript Poke script the TriggerNode runs.
	 * @param triggerNodeOut Set to the TriggerNode built. Owned by the hive.
	 * @param downstreamNodeOut Set to the downstream trigger target. Owned by the hive.
	 * @returns The hive, which the caller must shut down.
	 */
	GraphHive* _buildPokeTriggerHive(bool emitTriggerOnPoke, const std::string& pokeScript,
		TriggerNode*& triggerNodeOut, PokeTriggerCountingNode*& downstreamNodeOut)
	{
		GraphHive* hive = new GraphHive(2);

		triggerNodeOut = new TriggerNode("", pokeScript, emitTriggerOnPoke);
		downstreamNodeOut = new PokeTriggerCountingNode();

		hive -> addNode(triggerNodeOut);
		hive -> addNode(downstreamNodeOut);

		Handle<GraphNode> downstreamHandle(downstreamNodeOut);
		triggerNodeOut -> createEdge(downstreamHandle, {});

		triggerNodeOut -> setPokeEnabled(true);

		return hive;
	}

	void _hitPoke(GraphNode* node)
	{
		GraphPoke::PokeData data{};
		data.hitDuration = 100;

		node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));
	}
}

/**
 * A TriggerNode reports its own node type, so a trigger or an agent prompt restricted to TRIGGER_NODE can
 * single it out.
 */
TEST(TriggerNodeTest, ReportsTriggerNodeType)
{
	TriggerNode* node = new TriggerNode("", "");

	EXPECT_EQ(node -> getType(), GraphNodeType::TRIGGER_NODE);

	node -> decrRef();
}

/**
 * Poking a TriggerNode built to emit on poke runs its poke script and emits a trigger of its own. The two
 * are independent: the script runs whether or not anything downstream is listening, and the emitted
 * trigger reaches the downstream target without the script asking for it.
 */
TEST(TriggerNodeTest, PokeRunsPokeScriptAndEmitsTrigger)
{
	TriggerNode* triggerNode = nullptr;
	PokeTriggerCountingNode* downstreamNode = nullptr;

	GraphHive* hive = _buildPokeTriggerHive(true, "ranPokeScript = true", triggerNode, downstreamNode);
	Handle<GraphHive> hiveHandle(hive);

	_hitPoke(triggerNode);

	hive -> waitOnNoActionsActive(0);

	{ Handle<ScriptSession> sessionHandle = triggerNode -> requestPokeSession();

		bool ranPokeScript = false;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("ranPokeScript", ranPokeScript));
		EXPECT_TRUE(ranPokeScript) << "The poke script should have run on being poked.";
	}

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 1) << "Poking should have emitted a trigger that reached "
		"the connected downstream node.";

	hive -> shutdown();
}

/**
 * A TriggerNode built with emitTriggerOnPoke false still runs its poke script on every poke; only the
 * trigger the node would emit of its own accord is suppressed.
 */
TEST(TriggerNodeTest, PokeRunsPokeScriptWithoutEmittingWhenEmitOnPokeIsFalse)
{
	TriggerNode* triggerNode = nullptr;
	PokeTriggerCountingNode* downstreamNode = nullptr;

	GraphHive* hive = _buildPokeTriggerHive(false, "ranPokeScript = true", triggerNode, downstreamNode);
	Handle<GraphHive> hiveHandle(hive);

	_hitPoke(triggerNode);

	hive -> waitOnNoActionsActive(0);

	{ Handle<ScriptSession> sessionHandle = triggerNode -> requestPokeSession();

		bool ranPokeScript = false;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("ranPokeScript", ranPokeScript));
		EXPECT_TRUE(ranPokeScript) << "The poke script should run regardless of emitTriggerOnPoke.";
	}

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 0) << "No trigger should have been emitted by the node "
		"itself while emitTriggerOnPoke is false.";

	hive -> shutdown();
}

/**
 * With emission by the node suppressed, a poke script's own trigger() call still fires the subgraph, so
 * emitTriggerOnPoke false hands the decision to the script rather than disabling triggering outright.
 */
TEST(TriggerNodeTest, PokeScriptCanStillTriggerWhenEmitOnPokeIsFalse)
{
	TriggerNode* triggerNode = nullptr;
	PokeTriggerCountingNode* downstreamNode = nullptr;

	GraphHive* hive = _buildPokeTriggerHive(false, "if POKE_TYPE == 'HIT' then trigger() end",
		triggerNode, downstreamNode);
	Handle<GraphHive> hiveHandle(hive);

	_hitPoke(triggerNode);

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 1) << "The poke script's own trigger() should still have "
		"reached the downstream node.";

	hive -> shutdown();
}

/**
 * A TriggerNode with poking disabled discards the poke entirely: neither its poke script nor the trigger it
 * would emit runs.
 */
TEST(TriggerNodeTest, PokeIsDiscardedEntirelyWhenPokingDisabled)
{
	TriggerNode* triggerNode = nullptr;
	PokeTriggerCountingNode* downstreamNode = nullptr;

	GraphHive* hive = _buildPokeTriggerHive(true, "ranPokeScript = true", triggerNode, downstreamNode);
	Handle<GraphHive> hiveHandle(hive);

	triggerNode -> setPokeEnabled(false);

	_hitPoke(triggerNode);

	hive -> waitOnNoActionsActive(0);

	{ Handle<ScriptSession> sessionHandle = triggerNode -> requestPokeSession();

		bool ranPokeScript = false;
		EXPECT_FALSE(sessionHandle.getInstance() -> getGlobal("ranPokeScript", ranPokeScript))
			<< "The poke script should not have run while poking is disabled.";
	}

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 0) << "No trigger should have been emitted while poking "
		"is disabled.";

	hive -> shutdown();
}

#endif
