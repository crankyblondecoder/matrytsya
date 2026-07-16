#ifndef SCRIPT_NODE_UNIT_TEST_H
#define SCRIPT_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actionTargets/ScriptActionTarget.hpp"
#include "../../../graph/GraphPoke.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/StrobeScriptNode.hpp"

/**
 * StrobeScriptNode subclass that counts how many times _registerCoreGlobals() runs, so tests can assert
 * bindings are installed exactly once per node instance rather than on every invoke().
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
 * load precompiled bytecode, and allocation capped past a small budget. Since ScriptNode now owns its Lua
 * states privately, the only way to observe these properties from outside is through the public
 * ScriptActionTarget surface (invoke() + getGlobal()), exactly as a real script would experience them.
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

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());

	bool value = false;

	ASSERT_TRUE(target -> getGlobal("osIsNil", value)); EXPECT_TRUE(value) << "os library should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("ioIsNil", value)); EXPECT_TRUE(value) << "io library should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("dofileIsNil", value)); EXPECT_TRUE(value) << "dofile should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("loadfileIsNil", value)); EXPECT_TRUE(value) << "loadfile should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("printIsNil", value)); EXPECT_TRUE(value) << "print should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("requireIsNil", value)); EXPECT_TRUE(value) << "require should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("debugIsNil", value)); EXPECT_TRUE(value) << "debug library should not be available in the sandbox.";
	ASSERT_TRUE(target -> getGlobal("bytecodeBlocked", value)); EXPECT_TRUE(value) << "Loading precompiled bytecode should be blocked.";
	ASSERT_TRUE(target -> getGlobal("memoryLimited", value)); EXPECT_TRUE(value) << "Allocation past this state's memory budget should fail.";

	int computeResult = 0;
	ASSERT_TRUE(target -> getGlobal("computeResult", computeResult));
	EXPECT_EQ(computeResult, 42) << "Plain computation should still work in the sandbox.";

	node -> decrRef();
}

/**
 * A node's core state carries its global environment forward across invoke() calls, so a global one run
 * sets is still there, as the starting state, for the next run of the same script on the same node.
 */
TEST(ScriptNodeTest, CoreStateGlobalsPersistAcrossInvoke)
{
	ScriptNode* node = new ScriptNode(
		"wasSeenBefore = (counter ~= nil)\n"
		"counter = (counter or 0) + 1\n", "");

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());

	bool wasSeenBefore = true;
	int counter = 0;
	ASSERT_TRUE(target -> getGlobal("wasSeenBefore", wasSeenBefore));
	EXPECT_FALSE(wasSeenBefore) << "First invoke() should start from a clean environment.";
	ASSERT_TRUE(target -> getGlobal("counter", counter));
	EXPECT_EQ(counter, 1);

	ASSERT_TRUE(target -> invoke());

	ASSERT_TRUE(target -> getGlobal("wasSeenBefore", wasSeenBefore));
	EXPECT_TRUE(wasSeenBefore) << "A global set by one invoke() should survive into the next invoke().";
	ASSERT_TRUE(target -> getGlobal("counter", counter));
	EXPECT_EQ(counter, 2) << "The global's value should carry forward and accumulate across invokes.";

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

	bool ranPokeScript = false;
	EXPECT_FALSE(node -> getPokeGlobal("ranPokeScript", ranPokeScript))
		<< "Poke script should not have run while poking is disabled.";

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

	const char* seenType = nullptr;
	ASSERT_TRUE(node -> getPokeGlobal("seenType", seenType));
	EXPECT_STREQ(seenType, "HIT");

	int seenDuration = 0;
	ASSERT_TRUE(node -> getPokeGlobal("seenDuration", seenDuration));
	EXPECT_EQ(seenDuration, 750);

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

	const char* seenType = nullptr;
	ASSERT_TRUE(node -> getPokeGlobal("seenType", seenType));
	EXPECT_STREQ(seenType, "DRAG");

	double seenX = 0, seenY = 0, seenZ = 0;
	ASSERT_TRUE(node -> getPokeGlobal("seenX", seenX));
	ASSERT_TRUE(node -> getPokeGlobal("seenY", seenY));
	ASSERT_TRUE(node -> getPokeGlobal("seenZ", seenZ));

	EXPECT_DOUBLE_EQ(seenX, 1.5);
	EXPECT_DOUBLE_EQ(seenY, -2.5);
	EXPECT_DOUBLE_EQ(seenZ, 3.5);

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

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());

	GraphPoke::PokeData data{};
	node -> poke(GraphPoke(GraphPoke::PokeType::GRAB, data));

	bool pokeSeesCore = true;
	ASSERT_TRUE(node -> getPokeGlobal("pokeSeesCore", pokeSeesCore));
	EXPECT_FALSE(pokeSeesCore) << "Core script globals should not be visible to the poke script.";

	node -> decrRef();
}

/**
 * _registerCoreGlobals() must be called exactly once per node instance, the first time invoke() runs a
 * script, not on every invoke().
 */
TEST(ScriptNodeTest, CoreGlobalsAreRegisteredOnlyOnce)
{
	CountingStrobeScriptNode* node = new CountingStrobeScriptNode("x = 1", "");

	ScriptActionTarget* target = node -> getScriptActionTarget();

	ASSERT_TRUE(target -> invoke());
	ASSERT_TRUE(target -> invoke());
	ASSERT_TRUE(target -> invoke());

	EXPECT_EQ(node -> registrationCount, 1) << "_registerCoreGlobals() should only run once per node instance.";

	node -> decrRef();
}

/**
 * A binding registered by _registerCoreGlobals() must still be callable and still reflect live node
 * state on invokes after the first, even though it is only registered once.
 */
TEST(ScriptNodeTest, StrobeBindingPersistsAndReflectsLiveStateAcrossInvokes)
{
	CountingStrobeScriptNode* node = new CountingStrobeScriptNode("seenStrobe = getStrobe()", "");

	ScriptActionTarget* target = node -> getScriptActionTarget();

	node -> setStrobe(false);
	ASSERT_TRUE(target -> invoke());

	bool seenStrobe = true;
	ASSERT_TRUE(target -> getGlobal("seenStrobe", seenStrobe));
	EXPECT_FALSE(seenStrobe) << "getStrobe() should reflect the node's strobe flag on the first invoke.";

	node -> setStrobe(true);
	ASSERT_TRUE(target -> invoke());

	ASSERT_TRUE(target -> getGlobal("seenStrobe", seenStrobe));
	EXPECT_TRUE(seenStrobe) << "getStrobe() binding should still work and reflect live state on a later "
		"invoke, even though bindings are only registered once.";

	node -> decrRef();
}

#endif
