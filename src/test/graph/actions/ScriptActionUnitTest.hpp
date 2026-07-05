#ifndef SCRIPT_ACTION_UNIT_TEST_H
#define SCRIPT_ACTION_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../graph/actions/ScriptAction.hpp"
#include "../../../graph/actionTargets/ScriptActionTarget.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveHandle.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/GraphNodeHandle.hpp"
#include "../../../graph/graphActionFlagRegister.hpp"
#include "../../../lua/lua.hpp"

namespace
{
	bool isGlobalNil(lua_State* luaState, const char* name)
	{
		lua_getglobal(luaState, name);
		bool isNil = lua_isnil(luaState, -1);
		lua_pop(luaState, 1);
		return isNil;
	}
}

/**
 * Graph node that emits a ScriptAction of its own, mirroring how PingNode exposes emitPing().
 */
class ScriptEmitterNode : public GraphNode
{
	public:

		virtual ~ScriptEmitterNode() {}

		ScriptEmitterNode() : GraphNode()
		{
			_setEnergyCost(1);
		}

		/**
		 * Construct and emit a plain script action from this node.
		 * @param wait Wait for action to complete.
		 * @returns Script action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		ScriptAction* emitScript(bool wait)
		{
			GraphNodeHandle handle(this);

			return emit(new ScriptAction(handle), wait);
		}

		/**
		 * Emit an already constructed script action (e.g. a ScriptAction subclass bound to this node).
		 * @param action Action to emit.
		 * @param wait Wait for action to complete.
		 * @returns The same action, refincr'd. Caller must decref this to dispose.
		 */
		ScriptAction* emit(ScriptAction* action, bool wait)
		{
			action -> incrRef();

			_emitAction(action);

			if(wait) action -> waitOnComplete(0);

			return action;
		}
};

/**
 * Graph node that probes the Lua state it is invoked with, recording whether the sandbox holds: no OS,
 * filesystem or introspection access, memory usage capped, and no ability to load precompiled bytecode.
 */
class ScriptProbeNode : public GraphNode, public ScriptActionTarget
{
	public:

		virtual ~ScriptProbeNode() {}

		ScriptProbeNode() : GraphNode()
		{
			_setEnergyCost(1);
			_addActionFlag(SCRIPT_GRAPH_ACTION);
		}

		bool invoke(lua_State* luaState) override
		{
			_invoked = true;

			_osIsNil = isGlobalNil(luaState, "os");
			_ioIsNil = isGlobalNil(luaState, "io");
			_dofileIsNil = isGlobalNil(luaState, "dofile");
			_loadfileIsNil = isGlobalNil(luaState, "loadfile");
			_printIsNil = isGlobalNil(luaState, "print");
			_requireIsNil = isGlobalNil(luaState, "require");
			_debugIsNil = isGlobalNil(luaState, "debug");

			const char* script =
				"computeResult = 6 * 7\n"
				"local chunk = string.dump(function() return 1 end)\n"
				"local f = load(chunk, 'x', 'b')\n"
				"bytecodeBlocked = (f == nil)\n"
				"local ok = pcall(function() return string.rep('a', 5 * 1024 * 1024) end)\n"
				"memoryLimited = not ok\n";

			bool ranOk = (luaL_dostring(luaState, script) == LUA_OK);

			if(ranOk)
			{
				lua_getglobal(luaState, "computeResult");
				_computeResult = (int)lua_tointeger(luaState, -1);
				lua_pop(luaState, 1);

				lua_getglobal(luaState, "bytecodeBlocked");
				_bytecodeBlocked = lua_toboolean(luaState, -1);
				lua_pop(luaState, 1);

				lua_getglobal(luaState, "memoryLimited");
				_memoryLimited = lua_toboolean(luaState, -1);
				lua_pop(luaState, 1);
			}

			return ranOk;
		}

		ScriptActionTarget* getScriptActionTarget() override { return this; }

		bool wasInvoked() { return _invoked; }
		bool osIsNil() { return _osIsNil; }
		bool ioIsNil() { return _ioIsNil; }
		bool dofileIsNil() { return _dofileIsNil; }
		bool loadfileIsNil() { return _loadfileIsNil; }
		bool printIsNil() { return _printIsNil; }
		bool requireIsNil() { return _requireIsNil; }
		bool debugIsNil() { return _debugIsNil; }
		int getComputeResult() { return _computeResult; }
		bool bytecodeBlocked() { return _bytecodeBlocked; }
		bool memoryLimited() { return _memoryLimited; }

	private:

		bool _invoked = false;
		bool _osIsNil = false;
		bool _ioIsNil = false;
		bool _dofileIsNil = false;
		bool _loadfileIsNil = false;
		bool _printIsNil = false;
		bool _requireIsNil = false;
		bool _debugIsNil = false;
		int _computeResult = 0;
		bool _bytecodeBlocked = false;
		bool _memoryLimited = false;
};

/**
 * Graph node that writes a fixed integer to a named global when invoked.
 */
class ScriptGlobalWriterNode : public GraphNode, public ScriptActionTarget
{
	public:

		virtual ~ScriptGlobalWriterNode() {}

		ScriptGlobalWriterNode(const char* globalName, int value)
			: GraphNode(), _globalName(globalName), _value(value)
		{
			_setEnergyCost(1);
			_addActionFlag(SCRIPT_GRAPH_ACTION);
		}

		bool invoke(lua_State* luaState) override
		{
			lua_pushinteger(luaState, _value);
			lua_setglobal(luaState, _globalName);
			return true;
		}

		ScriptActionTarget* getScriptActionTarget() override { return this; }

	private:

		const char* _globalName;
		int _value;
};

/**
 * Graph node that records the value (or absence) of a named global when invoked.
 */
class ScriptGlobalReaderNode : public GraphNode, public ScriptActionTarget
{
	public:

		virtual ~ScriptGlobalReaderNode() {}

		ScriptGlobalReaderNode(const char* globalName) : GraphNode(), _globalName(globalName)
		{
			_setEnergyCost(1);
			_addActionFlag(SCRIPT_GRAPH_ACTION);
		}

		bool invoke(lua_State* luaState) override
		{
			_invoked = true;

			_wasNil = isGlobalNil(luaState, _globalName);

			if(!_wasNil)
			{
				lua_getglobal(luaState, _globalName);
				_value = (int)lua_tointeger(luaState, -1);
				lua_pop(luaState, 1);
			}

			return true;
		}

		ScriptActionTarget* getScriptActionTarget() override { return this; }

		bool wasInvoked() { return _invoked; }
		bool wasNil() { return _wasNil; }
		int getValue() { return _value; }

	private:

		const char* _globalName;
		bool _invoked = false;
		bool _wasNil = false;
		int _value = 0;
};

/**
 * ScriptAction subclass that publishes a value every node can read, exercising _shareGlobal() as the one
 * explicit, action-decided channel for state to cross between nodes.
 */
class SharingScriptAction : public ScriptAction
{
	public:

		SharingScriptAction(GraphNodeHandle& initNode) : ScriptAction(initNode)
		{
			_shareGlobal("shared", 99);
		}
};

TEST(ScriptActionTest, SandboxedStateRunsScriptWithNoOsAccess)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();
	ScriptProbeNode* probeNode = new ScriptProbeNode();

	hive -> addNode(sourceNode);
	hive -> addNode(probeNode);

	GraphNodeHandle probeHandle(probeNode);

	sourceNode -> createEdge(probeHandle);

	ScriptAction* action = sourceNode -> emitScript(true);

	ASSERT_TRUE(probeNode -> wasInvoked()) << "Probe node's script was never invoked.";

	EXPECT_TRUE(probeNode -> osIsNil()) << "os library should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> ioIsNil()) << "io library should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> dofileIsNil()) << "dofile should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> loadfileIsNil()) << "loadfile should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> printIsNil()) << "print should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> requireIsNil()) << "require should not be available in the sandbox.";
	EXPECT_TRUE(probeNode -> debugIsNil()) << "debug library should not be available in the sandbox.";

	EXPECT_EQ(probeNode -> getComputeResult(), 42) << "Plain computation should still work in the sandbox.";
	EXPECT_TRUE(probeNode -> bytecodeBlocked()) << "Loading precompiled bytecode should be blocked.";
	EXPECT_TRUE(probeNode -> memoryLimited()) << "Allocation past the action's memory budget should fail.";

	action -> decrRef();

	hive -> shutdown();
}

TEST(ScriptActionTest, GlobalsAreIsolatedPerNode)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();
	ScriptGlobalWriterNode* writerNode = new ScriptGlobalWriterNode("leaked", 123);
	ScriptGlobalReaderNode* readerNode = new ScriptGlobalReaderNode("leaked");

	hive -> addNode(sourceNode);
	hive -> addNode(writerNode);
	hive -> addNode(readerNode);

	GraphNodeHandle writerHandle(writerNode);
	GraphNodeHandle readerHandle(readerNode);

	sourceNode -> createEdge(writerHandle);
	writerNode -> createEdge(readerHandle);

	ScriptAction* action = sourceNode -> emitScript(true);

	ASSERT_TRUE(readerNode -> wasInvoked()) << "Reader node's script was never invoked.";

	EXPECT_TRUE(readerNode -> wasNil())
		<< "Global set by one node's script should not be visible to the next node's script.";

	action -> decrRef();

	hive -> shutdown();
}

TEST(ScriptActionTest, ExplicitlySharedGlobalsAreVisibleToEveryNode)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	ScriptEmitterNode* sourceNode = new ScriptEmitterNode();
	ScriptGlobalReaderNode* readerNode1 = new ScriptGlobalReaderNode("shared");
	ScriptGlobalReaderNode* readerNode2 = new ScriptGlobalReaderNode("shared");

	hive -> addNode(sourceNode);
	hive -> addNode(readerNode1);
	hive -> addNode(readerNode2);

	GraphNodeHandle reader1Handle(readerNode1);
	GraphNodeHandle reader2Handle(readerNode2);

	sourceNode -> createEdge(reader1Handle);
	readerNode1 -> createEdge(reader2Handle);

	GraphNodeHandle sourceHandle(sourceNode);
	SharingScriptAction* action = new SharingScriptAction(sourceHandle);

	sourceNode -> emit(action, true);

	ASSERT_TRUE(readerNode1 -> wasInvoked());
	ASSERT_TRUE(readerNode2 -> wasInvoked());

	EXPECT_FALSE(readerNode1 -> wasNil()) << "Explicitly shared global should be visible to every node.";
	EXPECT_EQ(readerNode1 -> getValue(), 99);

	EXPECT_FALSE(readerNode2 -> wasNil()) << "Explicitly shared global should be visible to every node.";
	EXPECT_EQ(readerNode2 -> getValue(), 99);

	action -> decrRef();

	hive -> shutdown();
}

#endif
