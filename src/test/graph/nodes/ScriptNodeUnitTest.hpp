#ifndef SCRIPT_NODE_UNIT_TEST_H
#define SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/GraphPoke.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../graph/nodes/StrobeScriptNode.hpp"
#include "../../../thread/ThreadException.hpp"

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

#endif
