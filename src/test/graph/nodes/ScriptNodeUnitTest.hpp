#ifndef SCRIPT_NODE_UNIT_TEST_H
#define SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include <atomic>

#include "../../../graph/actionTargets/TriggerActionTarget.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphPoke.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../graph/nodes/StrobeScriptNode.hpp"
#include "../../../lua/lua.hpp"
#include "../../../thread/ThreadException.hpp"
#include "../../../util/Handle.hpp"

/**
 * StrobeScriptNode subclass that counts how many times _registerCoreGlobals() runs, so tests can assert
 * bindings are installed exactly once per node instance rather than on every run.
 */
class CountingStrobeScriptNode : public StrobeScriptNode
{
	public:

		CountingStrobeScriptNode(const std::string& coreScript, const std::string& pokeScript)
			: StrobeScriptNode(coreScript, pokeScript) {}

		void strobe() override {}

		int registrationCount = 0;

	protected:

		void _registerCoreGlobals(lua_State* luaState) override
		{
			registrationCount++;
			StrobeScriptNode::_registerCoreGlobals(luaState);
		}
};

/**
 * ScriptNode subclass that keeps hold of the core state handed to _registerCoreGlobals(), so tests can
 * inspect that state directly. A node's states are private and reachable only through a session, which
 * deliberately exposes nothing of the Lua stack, so this is the only way to assert on stack depth - the one
 * property a session cannot report, and the one a leak shows up in.
 */
class StateCapturingScriptNode : public ScriptNode
{
	public:

		StateCapturingScriptNode(const std::string& coreScript, const std::string& pokeScript)
			: ScriptNode(coreScript, pokeScript) {}

		/// The node's core state, set on the first run and left alone afterwards.
		lua_State* capturedState = nullptr;

	protected:

		void _registerCoreGlobals(lua_State* luaState) override
		{
			capturedState = luaState;
			ScriptNode::_registerCoreGlobals(luaState);
		}
};

/**
 * ScriptNode subclass that can be triggered, counting each trigger it receives, so tests can observe a
 * TriggerAction emitted by another node's script arriving. The receiving half is stood up here rather than
 * using AgentNode, the one production trigger target, so these tests need no agentic harness behind them.
 */
class TriggerCountingScriptNode : public ScriptNode, public TriggerActionTarget
{
	public:

		TriggerCountingScriptNode() : ScriptNode("", "")
		{
			_addActionFlag(TRIGGER_GRAPH_ACTION);
		}

		void trigger() override { triggerCount++; }

		TriggerActionTarget* getTriggerActionTarget() override { return this; }

		/// Written from whichever worker thread applies the action, read from the test thread.
		std::atomic<int> triggerCount{0};
};

/**
 * A ScriptNode's core state should be sandboxed: no filesystem, process or introspection access, no way to
 * load precompiled bytecode, and allocation capped past a small budget. Since ScriptNode owns its Lua states
 * privately, the only way to observe these properties from outside is through a session, exactly as a real
 * script would experience them.
 */
TEST(ScriptNodeTest, CoreStateHasNoOsFilesystemOrIntrospectionAccess)
{
	ScriptNode* node = new ScriptNode(
		"osIsNil = (os == nil)\n"
		"ioIsNil = (io == nil)\n"
		"dofileIsNil = (dofile == nil)\n"
		"loadfileIsNil = (loadfile == nil)\n"
		"printIsNil = (print == nil)\n"
		"requireIsNil = (require == nil)\n"
		"debugIsNil = (debug == nil)\n"
		"computeResult = 6 * 7\n"
		"local chunk = string.dump(function() return 1 end)\n"
		"local f = load(chunk, 'x', 'b')\n"
		"bytecodeBlocked = (f == nil)\n"
		"local ok = pcall(function() return string.rep('a', 5 * 1024 * 1024) end)\n"
		"memoryLimited = not ok\n",
		"");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		bool value = false;

		ASSERT_TRUE(session -> getGlobal("osIsNil", value)); EXPECT_TRUE(value) << "os library should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("ioIsNil", value)); EXPECT_TRUE(value) << "io library should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("dofileIsNil", value)); EXPECT_TRUE(value) << "dofile should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("loadfileIsNil", value)); EXPECT_TRUE(value) << "loadfile should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("printIsNil", value)); EXPECT_TRUE(value) << "print should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("requireIsNil", value)); EXPECT_TRUE(value) << "require should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("debugIsNil", value)); EXPECT_TRUE(value) << "debug library should not be available in the sandbox.";
		ASSERT_TRUE(session -> getGlobal("bytecodeBlocked", value)); EXPECT_TRUE(value) << "Loading precompiled bytecode should be blocked.";
		ASSERT_TRUE(session -> getGlobal("memoryLimited", value)); EXPECT_TRUE(value) << "Allocation past this state's memory budget should fail.";

		int computeResult = 0;
		ASSERT_TRUE(session -> getGlobal("computeResult", computeResult));
		EXPECT_EQ(computeResult, 42) << "Plain computation should still work in the sandbox.";
	}

	node -> decrRef();
}

/**
 * A node's core state carries its global environment forward across runs, so a global one run sets is still
 * there, as the starting state, for the next run of the same script on the same node - including when that
 * next run happens in a later session.
 */
TEST(ScriptNodeTest, CoreStateGlobalsPersistAcrossRuns)
{
	ScriptNode* node = new ScriptNode(
		"wasSeenBefore = (counter ~= nil)\n"
		"counter = (counter or 0) + 1\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		bool wasSeenBefore = true;
		int counter = 0;
		ASSERT_TRUE(session -> getGlobal("wasSeenBefore", wasSeenBefore));
		EXPECT_FALSE(wasSeenBefore) << "First run should start from a clean environment.";
		ASSERT_TRUE(session -> getGlobal("counter", counter));
		EXPECT_EQ(counter, 1);
	}

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		bool wasSeenBefore = false;
		int counter = 0;
		ASSERT_TRUE(session -> getGlobal("wasSeenBefore", wasSeenBefore));
		EXPECT_TRUE(wasSeenBefore) << "A global set by one run should survive into the next session's run.";
		ASSERT_TRUE(session -> getGlobal("counter", counter));
		EXPECT_EQ(counter, 2) << "The global's value should carry forward and accumulate across runs.";
	}

	node -> decrRef();
}

/**
 * A global staged through a session must be visible to a script run in that same session, and must still be
 * there for the next session, so an action can publish values into a node and have the script it then runs
 * read them without anything else interleaving.
 */
TEST(ScriptNodeTest, StagedGlobalsAreVisibleToTheRunInTheSameSession)
{
	ScriptNode* node = new ScriptNode("doubled = staged * 2", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		session -> setGlobal("staged", 21);

		ASSERT_TRUE(session -> run());

		int doubled = 0;
		ASSERT_TRUE(session -> getGlobal("doubled", doubled));
		EXPECT_EQ(doubled, 42) << "The script should see a global staged earlier in the same session.";
	}

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		int staged = 0;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("staged", staged));
		EXPECT_EQ(staged, 21) << "A staged global should still be readable from a later session.";
	}

	node -> decrRef();
}

/**
 * A core script that defines neither entry point is run in full, top to bottom, on every run, exactly as one
 * written before either entry point existed. This is the baseline the whole lifecycle is layered on top of.
 */
TEST(ScriptNodeTest, CoreScriptWithNeitherEntryPointRunsItsWholeChunkEveryRun)
{
	ScriptNode* node = new ScriptNode("chunkRuns = (chunkRuns or 0) + 1", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());

		int chunkRuns = 0;
		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 3) << "A script defining no invoke() should have its chunk run on every run.";
	}

	node -> decrRef();
}

/**
 * The central lifecycle contract: init() is called once and once only, invoke() is called on every run, and
 * the presence of an invoke() stops the chunk being run again after the run that defined it.
 */
TEST(ScriptNodeTest, CoreScriptInitRunsOnceAndInvokeRunsEveryRun)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function init() initRuns = (initRuns or 0) + 1 end\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());

		int chunkRuns = 0, initRuns = 0, invokeRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1) << "The chunk should not be run again once an invoke() has been defined.";

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "init() should be called exactly once per node instance.";

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 3) << "invoke() should be called on every run.";
	}

	node -> decrRef();
}

/**
 * Skipping the chunk has to be safe as well as cheap: a top level local the chunk built is held by invoke()
 * as an upvalue, so it is still reachable on a run for which the chunk was never loaded at all.
 */
TEST(ScriptNodeTest, CoreScriptInvokeReachesTopLevelLocalsAsUpvalues)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"local helper = function() return 7 end\n"
		"function invoke() value = helper() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run()) << "A run whose chunk is skipped should still reach the chunk's locals.";

		int chunkRuns = 0, invokeRuns = 0, value = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1);

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 2);

		ASSERT_TRUE(session -> getGlobal("value", value));
		EXPECT_EQ(value, 7) << "A top level local should still be callable as invoke()'s upvalue.";
	}

	node -> decrRef();
}

/**
 * Defining either entry point closes the chunk down, so a script whose only work is a one-off build needs
 * nothing but an init(): its chunk runs once to define that init(), the init() runs once, and every run
 * after that does nothing at all rather than rebuilding the chunk's helpers for no reason.
 */
TEST(ScriptNodeTest, CoreScriptWithInitOnlyRunsNothingAfterTheRunThatCalledIt)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function init() initRuns = (initRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());

		int chunkRuns = 0, initRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1) << "Defining init() alone should stop the chunk being run again.";

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "init() should be called only once.";
	}

	node -> decrRef();
}

/**
 * An init() that raises still counts as defined, so it still closes the chunk down. The run it raised on
 * fails, and every run after it does nothing rather than re-running the chunk and building the node a second
 * time - which is exactly what the one-shot flag exists to prevent.
 */
TEST(ScriptNodeTest, CoreScriptWithRaisingInitOnlyDoesNotBringTheChunkBack)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function init() initRuns = (initRuns or 0) + 1 error('init failed') end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		EXPECT_FALSE(session -> run());
		EXPECT_TRUE(session -> run()) << "A later run has nothing left to do, so nothing left to fail.";
		EXPECT_TRUE(session -> run());

		int chunkRuns = 0, initRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1) << "A raising init() should still close the chunk down.";

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "A raising init() should never be retried.";
	}

	node -> decrRef();
}

/**
 * The two entry points are independent: a script may define invoke() without init(), and nothing goes looking
 * for the one it did not define.
 */
TEST(ScriptNodeTest, CoreScriptWithInvokeOnlyIsCalledEveryRun)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());

		int chunkRuns = 0, invokeRuns = 0, initRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1);

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 3);

		EXPECT_FALSE(session -> getGlobal("initRuns", initRuns)) << "An undefined init() should simply be absent.";
	}

	node -> decrRef();
}

/**
 * init() gets exactly one attempt whatever comes of it. An init() that raises fails the run it was called on
 * and holds invoke() back for that run only; it is never retried, and later runs go straight on to invoke().
 */
TEST(ScriptNodeTest, CoreScriptRaisingInitConsumesItsOneShotAndLaterInvokesStillRun)
{
	ScriptNode* node = new ScriptNode(
		"function init() initRuns = (initRuns or 0) + 1 error('init failed') end\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		EXPECT_FALSE(session -> run()) << "A run whose init() raised should report failure.";

		int initRuns = 0, invokeRuns = 0;

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1);
		EXPECT_FALSE(session -> getGlobal("invokeRuns", invokeRuns))
			<< "invoke() should be held back on the run whose init() raised.";

		EXPECT_TRUE(session -> run()) << "A later run should succeed, init() having already had its attempt.";

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "A raising init() should never be retried.";

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 1);

		EXPECT_TRUE(session -> run());

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 2);
	}

	node -> decrRef();
}

/**
 * A run that fails in the chunk itself never reaches init(), so it must not spend init()'s one attempt on a
 * run that never offered it.
 */
TEST(ScriptNodeTest, CoreScriptFailingChunkDoesNotConsumeInitsOneShot)
{
	ScriptNode* node = new ScriptNode(
		"if not primed then error('not primed') end\n"
		"function init() initRuns = (initRuns or 0) + 1 end\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		EXPECT_FALSE(session -> run()) << "A chunk that raises should report failure.";

		int initRuns = 0, invokeRuns = 0;
		EXPECT_FALSE(session -> getGlobal("initRuns", initRuns)) << "init() should not have been reached at all.";

		session -> setGlobal("primed", true);

		EXPECT_TRUE(session -> run());

		ASSERT_TRUE(session -> getGlobal("initRuns", initRuns));
		EXPECT_EQ(initRuns, 1) << "init()'s attempt should have survived the run that never reached it.";

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 1);
	}

	node -> decrRef();
}

/**
 * Only a function counts as an entry point. A global named invoke that holds something else is left alone,
 * and the script falls back to being run in full, as though it had defined nothing.
 */
TEST(ScriptNodeTest, CoreScriptNonFunctionInvokeGlobalIsIgnored)
{
	ScriptNode* node = new ScriptNode(
		"invoke = 42\n"
		"chunkRuns = (chunkRuns or 0) + 1\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());

		int chunkRuns = 0, invoke = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 3) << "A non-function invoke should not stop the chunk being run.";

		ASSERT_TRUE(session -> getGlobal("invoke", invoke));
		EXPECT_EQ(invoke, 42) << "A non-function invoke should be left exactly as the script set it.";
	}

	node -> decrRef();
}

/**
 * An invoke() that raises fails its run without the chunk being run again to replace it: a failing entry
 * point stays the entry point.
 */
TEST(ScriptNodeTest, CoreScriptRaisingInvokeFailsEveryRunWithoutReRunningTheChunk)
{
	ScriptNode* node = new ScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 error('boom') end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		EXPECT_FALSE(session -> run());
		EXPECT_FALSE(session -> run());

		int chunkRuns = 0, invokeRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 1) << "A raising invoke() should not bring the chunk back.";

		ASSERT_TRUE(session -> getGlobal("invokeRuns", invokeRuns));
		EXPECT_EQ(invokeRuns, 2) << "A raising invoke() should still be called on every run.";
	}

	node -> decrRef();
}

/**
 * The lifecycle is the core script's alone. A poke script is run in full on every poke, and the two entry
 * point names mean nothing in the poke state.
 */
TEST(ScriptNodeTest, PokeScriptHasNoInitOrInvokeLifecycle)
{
	ScriptNode* node = new ScriptNode("",
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function init() initRuns = (initRuns or 0) + 1 end\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n");

	node -> setPokeEnabled(true);

	GraphPoke::PokeData data{};

	node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));
	node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));
	node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));

	{ Handle<ScriptSession> sessionHandle = node -> requestPokeSession();

		ScriptSession* session = sessionHandle.getInstance();

		int chunkRuns = 0, initRuns = 0, invokeRuns = 0;

		ASSERT_TRUE(session -> getGlobal("chunkRuns", chunkRuns));
		EXPECT_EQ(chunkRuns, 3) << "A poke script should be run in full on every poke.";

		EXPECT_FALSE(session -> getGlobal("initRuns", initRuns)) << "init() should never be called on a poke.";
		EXPECT_FALSE(session -> getGlobal("invokeRuns", invokeRuns)) << "invoke() should never be called on a poke.";
	}

	node -> decrRef();
}

/**
 * Every run must leave its state's stack exactly as it found it, whether the script succeeded or failed.
 * A failed lua_pcall does not unwind to where it was called from: it replaces the function it was given with
 * the error object it produced, so a failing run that does not pop leaves one value behind. These states live
 * as long as their node does, and a broken script fails on every strobe, so an unpopped error object grows
 * the stack without bound until the state's memory budget is exhausted and the node is dead for good.
 *
 * The stack depth is checked directly rather than inferred from a run failing or a later allocation being
 * refused, since a state exhausted by leaked slots reports both of those exactly as a healthy one does.
 */
TEST(ScriptNodeTest, EveryCoreRunLeavesTheStackBalanced)
{
	StateCapturingScriptNode* node = new StateCapturingScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function init() error('init raised') end\n"
		"function invoke() error('invoke raised') end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		// The first run reaches the chunk, then a raising init(); every run after it a raising invoke(). Each
		// path leaves an error object behind that only this class pops.
		EXPECT_FALSE(session -> run());

		ASSERT_NE(node -> capturedState, nullptr) << "The core state should have been captured by now.";
		EXPECT_EQ(lua_gettop(node -> capturedState), 0) << "A run whose init() raised should leave no value behind.";

		EXPECT_FALSE(session -> run());
		EXPECT_EQ(lua_gettop(node -> capturedState), 0) << "A run whose invoke() raised should leave no value behind.";

		EXPECT_FALSE(session -> run());
		EXPECT_EQ(lua_gettop(node -> capturedState), 0) << "Failed runs should not accumulate on the stack.";
	}

	node -> decrRef();
}

/**
 * A run that fails to load its chunk at all, and a run that succeeds outright, must balance the stack too -
 * the failed load and the clean run being the two paths the failing-entry-point test above does not take.
 */
TEST(ScriptNodeTest, SucceedingCoreRunLeavesTheStackBalanced)
{
	StateCapturingScriptNode* node = new StateCapturingScriptNode(
		"chunkRuns = (chunkRuns or 0) + 1\n"
		"function invoke() invokeRuns = (invokeRuns or 0) + 1 end\n", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		ASSERT_NE(node -> capturedState, nullptr);
		EXPECT_EQ(lua_gettop(node -> capturedState), 0) << "The run that loaded the chunk should balance the stack.";

		ASSERT_TRUE(session -> run());
		EXPECT_EQ(lua_gettop(node -> capturedState), 0) << "A run entered through invoke() should balance the stack.";
	}

	node -> decrRef();
}

/**
 * A state can only be held by one session at a time. A thread asking for a second session on a state it
 * already holds is a programming error and is reported as one rather than deadlocking, and the session it
 * already holds is left working.
 */
TEST(ScriptNodeTest, SecondSessionOnAHeldStateIsRejected)
{
	ScriptNode* node = new ScriptNode("ran = true", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		EXPECT_THROW(node -> requestCoreSession(), ThreadException)
			<< "Requesting a second session on a state this thread already holds should be rejected.";

		ASSERT_TRUE(sessionHandle.getInstance() -> run()) << "The held session should be unaffected.";

		// The poke state locks independently, so a session on it may be held at the same time.
		{ Handle<ScriptSession> pokeSessionHandle = node -> requestPokeSession();

			EXPECT_TRUE(pokeSessionHandle.isValid()) << "The poke state should not be blocked by a core session.";
		}
	}

	// The rejected request must not have consumed the lock: a session can still be had once the first is done.
	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		bool ran = false;
		EXPECT_TRUE(sessionHandle.getInstance() -> getGlobal("ran", ran));
	}

	node -> decrRef();
}

/**
 * A poked node with poking disabled discards the poke; its poke script never runs.
 */
TEST(ScriptNodeTest, PokeIsDiscardedWhenPokingDisabled)
{
	ScriptNode* node = new ScriptNode("", "ranPokeScript = true");

	GraphPoke::PokeData data{};
	data.hitDuration = 250;

	node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));

	{ Handle<ScriptSession> sessionHandle = node -> requestPokeSession();

		bool ranPokeScript = false;
		EXPECT_FALSE(sessionHandle.getInstance() -> getGlobal("ranPokeScript", ranPokeScript))
			<< "Poke script should not have run while poking is disabled.";
	}

	node -> decrRef();
}

/**
 * A HIT poke exposes POKE_TYPE and HIT_DURATION to the poke script.
 */
TEST(ScriptNodeTest, PokeScriptSeesHitContext)
{
	ScriptNode* node = new ScriptNode("", "seenType = POKE_TYPE\nseenDuration = HIT_DURATION");

	node -> setPokeEnabled(true);

	GraphPoke::PokeData data{};
	data.hitDuration = 750;

	node -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));

	{ Handle<ScriptSession> sessionHandle = node -> requestPokeSession();

		ScriptSession* session = sessionHandle.getInstance();

		const char* seenType = nullptr;
		ASSERT_TRUE(session -> getGlobal("seenType", seenType));
		EXPECT_STREQ(seenType, "HIT");

		int seenDuration = 0;
		ASSERT_TRUE(session -> getGlobal("seenDuration", seenDuration));
		EXPECT_EQ(seenDuration, 750);
	}

	node -> decrRef();
}

/**
 * A DRAG poke exposes POKE_TYPE and DRAG_VECTOR to the poke script.
 */
TEST(ScriptNodeTest, PokeScriptSeesDragContext)
{
	ScriptNode* node = new ScriptNode("",
		"seenType = POKE_TYPE\n"
		"seenX = DRAG_VECTOR[1]\n"
		"seenY = DRAG_VECTOR[2]\n"
		"seenZ = DRAG_VECTOR[3]\n");

	node -> setPokeEnabled(true);

	GraphPoke::PokeData data{};
	data.dragVector[0] = 1.5f;
	data.dragVector[1] = -2.5f;
	data.dragVector[2] = 3.5f;

	node -> poke(GraphPoke(GraphPoke::PokeType::DRAG, data));

	{ Handle<ScriptSession> sessionHandle = node -> requestPokeSession();

		ScriptSession* session = sessionHandle.getInstance();

		const char* seenType = nullptr;
		ASSERT_TRUE(session -> getGlobal("seenType", seenType));
		EXPECT_STREQ(seenType, "DRAG");

		double seenX = 0, seenY = 0, seenZ = 0;
		ASSERT_TRUE(session -> getGlobal("seenX", seenX));
		ASSERT_TRUE(session -> getGlobal("seenY", seenY));
		ASSERT_TRUE(session -> getGlobal("seenZ", seenZ));

		EXPECT_DOUBLE_EQ(seenX, 1.5);
		EXPECT_DOUBLE_EQ(seenY, -2.5);
		EXPECT_DOUBLE_EQ(seenZ, 3.5);
	}

	node -> decrRef();
}

/**
 * The core and poke scripts run against fully independent states: a plain global set by one is never
 * visible to the other.
 */
TEST(ScriptNodeTest, CoreAndPokeStatesAreIndependent)
{
	ScriptNode* node = new ScriptNode("coreOnly = 111", "pokeSeesCore = (coreOnly ~= nil)");

	node -> setPokeEnabled(true);

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	GraphPoke::PokeData data{};
	node -> poke(GraphPoke(GraphPoke::PokeType::GRAB, data));

	{ Handle<ScriptSession> sessionHandle = node -> requestPokeSession();

		bool pokeSeesCore = true;
		ASSERT_TRUE(sessionHandle.getInstance() -> getGlobal("pokeSeesCore", pokeSeesCore));
		EXPECT_FALSE(pokeSeesCore) << "Core script globals should not be visible to the poke script.";
	}

	node -> decrRef();
}

/**
 * _registerCoreGlobals() must be called exactly once per node instance, the first time a core session runs a
 * script, not on every run.
 */
TEST(ScriptNodeTest, CoreGlobalsAreRegisteredOnlyOnce)
{
	CountingStrobeScriptNode* node = new CountingStrobeScriptNode("x = 1", "");

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
		ASSERT_TRUE(session -> run());
	}

	EXPECT_EQ(node -> registrationCount, 1) << "_registerCoreGlobals() should only run once per node instance.";

	node -> decrRef();
}

/**
 * A binding registered by _registerCoreGlobals() must still be callable and still reflect live node
 * state on runs after the first, even though it is only registered once.
 */
TEST(ScriptNodeTest, StrobeBindingPersistsAndReflectsLiveStateAcrossRuns)
{
	CountingStrobeScriptNode* node = new CountingStrobeScriptNode("seenStrobe = getStrobe()", "");

	node -> setStrobe(false);

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		bool seenStrobe = true;
		ASSERT_TRUE(session -> getGlobal("seenStrobe", seenStrobe));
		EXPECT_FALSE(seenStrobe) << "getStrobe() should reflect the node's strobe flag on the first run.";
	}

	node -> setStrobe(true);

	{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		ASSERT_TRUE(session -> run());

		bool seenStrobe = false;
		ASSERT_TRUE(session -> getGlobal("seenStrobe", seenStrobe));
		EXPECT_TRUE(seenStrobe) << "getStrobe() binding should still work and reflect live state on a later "
			"run, even though bindings are only registered once.";
	}

	node -> decrRef();
}

/**
 * trigger() called from a core script emits a TriggerAction from that node, which traverses the graph and
 * triggers every connected trigger target it reaches. This is the mechanism a script uses to fire a
 * subgraph from a single call.
 */
TEST(ScriptNodeTest, TriggerFromCoreScriptReachesConnectedNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("trigger()", "");
	TriggerCountingScriptNode* downstreamNode = new TriggerCountingScriptNode();

	hive -> addNode(emittingNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	emittingNode -> createEdge(downstreamHandle, {});

	// Running the script directly, rather than via a ScriptAction, keeps the emitted TriggerAction as the
	// only action traversing the hive. It is started synchronously from inside the run, so it is already
	// registered active by the time the session is released below.
	{ Handle<ScriptSession> sessionHandle = emittingNode -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 1) << "TriggerAction emitted by the core script should "
		"have reached the connected downstream node exactly once.";

	hive -> shutdown();
}

/**
 * The trigger() binding is registered into the poke state as well as the core state, so a poke script can
 * fire a subgraph in response to a poke.
 */
TEST(ScriptNodeTest, TriggerFromPokeScriptReachesConnectedNode)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("", "if POKE_TYPE == 'HIT' then trigger() end");
	TriggerCountingScriptNode* downstreamNode = new TriggerCountingScriptNode();

	hive -> addNode(emittingNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	emittingNode -> createEdge(downstreamHandle, {});

	emittingNode -> setPokeEnabled(true);

	GraphPoke::PokeData data{};
	data.hitDuration = 100;

	emittingNode -> poke(GraphPoke(GraphPoke::PokeType::HIT, data));

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 1) << "TriggerAction emitted by the poke script should "
		"have reached the connected downstream node.";

	hive -> shutdown();
}

/**
 * The optional arguments to trigger() restrict which nodes the emitted action fires: only nodes matching
 * the given name, and only nodes of the given NodeType, are triggered.
 */
TEST(ScriptNodeTest, TriggerRestrictsByNodeNameAndType)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("trigger('wanted', NodeType.SCRIPT_NODE)", "");

	TriggerCountingScriptNode* wantedNode = new TriggerCountingScriptNode();
	TriggerCountingScriptNode* unwantedNode = new TriggerCountingScriptNode();

	wantedNode -> setName("wanted");
	unwantedNode -> setName("unwanted");

	hive -> addNode(emittingNode);
	hive -> addNode(wantedNode);
	hive -> addNode(unwantedNode);

	Handle<GraphNode> wantedHandle(wantedNode);
	Handle<GraphNode> unwantedHandle(unwantedNode);

	emittingNode -> createEdge(wantedHandle, {});
	emittingNode -> createEdge(unwantedHandle, {});

	{ Handle<ScriptSession> sessionHandle = emittingNode -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(wantedNode -> triggerCount.load(), 1) << "The node the trigger named should have been triggered.";
	EXPECT_EQ(unwantedNode -> triggerCount.load(), 0) << "A node the trigger did not name should have been "
		"traversed but left untriggered.";

	hive -> shutdown();
}

/**
 * A node type that is not one of the NodeType constants cannot be honoured, so trigger() raises a Lua
 * error rather than silently falling back to triggering everything.
 */
TEST(ScriptNodeTest, TriggerWithUnrecognisedNodeTypeFails)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* emittingNode = new ScriptNode("trigger('', 9999)\nreachedEnd = true", "");
	TriggerCountingScriptNode* downstreamNode = new TriggerCountingScriptNode();

	hive -> addNode(emittingNode);
	hive -> addNode(downstreamNode);

	Handle<GraphNode> downstreamHandle(downstreamNode);
	emittingNode -> createEdge(downstreamHandle, {});

	{ Handle<ScriptSession> sessionHandle = emittingNode -> requestCoreSession();

		ScriptSession* session = sessionHandle.getInstance();

		EXPECT_FALSE(session -> run()) << "An unrecognised NodeType value should fail the run.";

		bool reachedEnd = false;
		EXPECT_FALSE(session -> getGlobal("reachedEnd", reachedEnd))
			<< "The script should not have continued past the failed trigger() call.";
	}

	hive -> waitOnNoActionsActive(0);

	EXPECT_EQ(downstreamNode -> triggerCount.load(), 0) << "No TriggerAction should have been emitted.";

	hive -> shutdown();
}

#endif
