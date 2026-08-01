#include "ScriptNode.hpp"

#include "ScriptSession.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphException.hpp"
#include "../GraphPoke.hpp"
#include "../actions/TriggerAction.hpp"

#include "../../lua/lua.hpp"
#include "../../log/log.hpp"
#include "../../thread/ThreadException.hpp"

#include <cmath>
#include <cstdlib>

namespace
{
	// Globals the base library opens with that reach outside the sandbox (filesystem or stdio); stripped once
	// each state is created.
	const char* const DISALLOWED_GLOBALS[] = {"dofile", "loadfile", "print", "warn"};
}

ScriptNode::~ScriptNode()
{
	if(_coreLuaState) lua_close(_coreLuaState);
	if(_pokeLuaState) lua_close(_pokeLuaState);
}

ScriptNode::ScriptNode(const std::string& coreScript, const std::string& pokeScript)
	: GraphSerialisedActionNode(), _coreScript{coreScript}, _pokeScript(pokeScript)
{
	_setEnergyCost(1);

	// Supports script action.
	_addActionFlag(SCRIPT_GRAPH_ACTION);

	__compileCoreScript();
	__compilePokeScript();

	_coreLuaState = __createSandboxedState(&_coreMemoryUsed, &_coreBaseEnvRef);
	_pokeLuaState = __createSandboxedState(&_pokeMemoryUsed, &_pokeBaseEnvRef);

	// Prime both states with a fresh environment up front, so a session's setGlobal() and
	// _registerCoreGlobals() have a live fresh table to write into even before the first run.
	__installFreshEnv(_coreLuaState, _coreBaseEnvRef);
	__installFreshEnv(_pokeLuaState, _pokeBaseEnvRef);
}

GraphNode::Type ScriptNode::getType()
{
	return Type::SCRIPT_NODE;
}

Handle<ScriptSession> ScriptNode::requestCoreSession()
{
	_coreLock.lock();

	ScriptSession* session = 0;

	try
	{
		session = new ScriptSession(this, _coreLuaState, false);
	}
	catch(...)
	{
		_coreLock.unlock();
		throw;
	}

	Handle<ScriptSession> sessionHandle(session);

	// The reference the session was constructed with belongs to the handle now.
	session -> decrRef();

	return sessionHandle;
}

Handle<ScriptSession> ScriptNode::requestPokeSession()
{
	_pokeLock.lock();

	ScriptSession* session = 0;

	try
	{
		session = new ScriptSession(this, _pokeLuaState, true);
	}
	catch(...)
	{
		_pokeLock.unlock();
		throw;
	}

	Handle<ScriptSession> sessionHandle(session);

	// The reference the session was constructed with belongs to the handle now.
	session -> decrRef();

	return sessionHandle;
}

bool ScriptNode::__runCore()
{
	if(_coreBytecode.empty()) return false;

	// Note: The session's lock is deliberately still held across the script run itself. Nothing else may
	// touch the core state while a script is running against it, and a script's callbacks only ever reach
	// back into this node's other state (vertexes, transform, animating flag), never into the core state, so
	// no call made from inside the run can arrive back here.

	__registerCoreGlobalsOnce();

	bool success = luaL_loadbufferx(_coreLuaState, _coreBytecode.data(), _coreBytecode.size(), "script", "b") == LUA_OK;

	// Mode "b" only accepts bytecode. _coreBytecode is compiled from _coreScript once, at construction, by
	// this class itself rather than supplied by the script being run, so it never crosses the trust boundary
	// that the "t"-only loading elsewhere in this module guards against.
	if(success)
	{
		success = lua_pcall(_coreLuaState, 0, 0, 0) == LUA_OK;
	}
	else
	{
		lua_pop(_coreLuaState, 1);
	}

	// Note: lua_pcall already pops the function and any error message off the stack on failure, unlike
	// luaL_loadbufferx, which leaves an error message on the stack that has to be popped explicitly.

	// The environment the script just ran against is left live rather than replaced, so any global it set (or
	// that the session staged ahead of this run) is still there, as the starting state, for the next session.
	return success;
}

bool ScriptNode::__runPoke()
{
	if(_pokeBytecode.empty()) return false;

	// Note: As in __runCore(), the session's lock is still held across the script run so that nothing else
	// drives the poke state while a poke script is running against it.

	__registerPokeGlobalsOnce();

	bool success = luaL_loadbufferx(_pokeLuaState, _pokeBytecode.data(), _pokeBytecode.size(), "script", "b") == LUA_OK;

	if(success)
	{
		success = lua_pcall(_pokeLuaState, 0, 0, 0) == LUA_OK;
	}
	else
	{
		lua_pop(_pokeLuaState, 1);
	}

	// The environment the poke script just ran against is left live rather than replaced, so any global it
	// set is still there, as the starting state, the next time this node is poked.
	return success;
}

void ScriptNode::__releaseState(bool poke)
{
	try
	{
		if(poke) _pokeLock.unlock();
		else _coreLock.unlock();
	}
	catch(ThreadException& ex)
	{
		// A session is released from its destructor, so there is nowhere for this to be thrown to. It only
		// happens if the session outlived the thread that requested it, which is not permitted.
		LOG(Logger::LogLevel::ERROR, "Could not release script state lock.")
	}
}

ScriptActionTarget* ScriptNode::getScriptActionTarget()
{
	return this;
}

void ScriptNode::_poked(GraphPoke poke)
{
	try
	{
		{ Handle<ScriptSession> sessionHandle = requestPokeSession();

			ScriptSession* session = sessionHandle.getInstance();

			// The poke's contents are staged as globals ahead of the run so the script can branch on what
			// kind of poke this is.
			session -> setGlobal("POKE_TYPE", __pokeTypeName(poke.getType()));
			session -> setGlobal("HIT_DURATION", poke.getHitDuration());

			float dragVector[3];
			poke.getDragVector(dragVector);

			session -> setGlobal("DRAG_VECTOR", dragVector, 3);

			session -> run();
		}
	}
	catch(ThreadException& ex)
	{
		// A poke that can't claim the poke state is dropped rather than propagated back into the graph.
		LOG(Logger::LogLevel::DEBUG, "Poke dropped; could not obtain a session on the poke state.")
	}
}

void ScriptNode::__compileCoreScript()
{
	lua_State* scratchState = luaL_newstate();

	if(!scratchState) return;

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(scratchState, _coreScript.c_str(), _coreScript.size(), "script", "t") == LUA_OK)
	{
		// Strip debug info: __runCore() never inspects line numbers or names from a failed pcall.
		lua_dump(scratchState, __writeBytecode, &_coreBytecode, 1);
	}

	lua_close(scratchState);
}

void ScriptNode::__compilePokeScript()
{
	lua_State* scratchState = luaL_newstate();

	if(!scratchState) return;

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(scratchState, _pokeScript.c_str(), _pokeScript.size(), "script", "t") == LUA_OK)
	{
		// Strip debug info: __runPoke() never inspects line numbers or names from a failed pcall.
		lua_dump(scratchState, __writeBytecode, &_pokeBytecode, 1);
	}

	lua_close(scratchState);
}

int ScriptNode::__writeBytecode(lua_State*, const void* data, size_t size, void* userData)
{
	static_cast<std::string*>(userData) -> append(static_cast<const char*>(data), size);
	return 0;
}

lua_State* ScriptNode::__createSandboxedState(size_t* memoryUsed, int* baseEnvRef)
{
	lua_State* luaState = lua_newstate(__alloc, memoryUsed, luaL_makeseed(0));

	if(!luaState) throw GraphException(GraphException::SCRIPT_STATE_BAD_ALLOC);

	// Only libraries with no filesystem, process, environment or introspection access are opened. io, os,
	// package and debug are deliberately left closed.
	luaL_openselectedlibs(luaState,
		LUA_GLIBK | LUA_COLIBK | LUA_MATHLIBK | LUA_STRLIBK | LUA_TABLIBK | LUA_UTF8LIBK, 0);

	for(const char* global : DISALLOWED_GLOBALS)
	{
		lua_pushnil(luaState);
		lua_setglobal(luaState, global);
	}

	lua_pushcfunction(luaState, __safeLoad);
	lua_setglobal(luaState, "load");

	// Keep this table around as the clean base the live env reads through to; it is never
	// installed as the live global table again after this point.
	lua_pushglobaltable(luaState);
	*baseEnvRef = luaL_ref(luaState, LUA_REGISTRYINDEX);

	return luaState;
}

void ScriptNode::__installFreshEnv(lua_State* luaState, int baseEnvRef)
{
	lua_newtable(luaState); // env
	lua_newtable(luaState); // metatable
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, baseEnvRef); // base table
	lua_setfield(luaState, -2, "__index"); // metatable.__index = base table
	lua_setmetatable(luaState, -2); // env's metatable = metatable

	// Reads not found in env fall through to the base table; writes only ever land in env.
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}

void ScriptNode::__registerCoreGlobalsOnce()
{
	__registerGlobalsOnce(_coreLuaState, _coreBaseEnvRef, _coreGlobalsRegistered);
}

void ScriptNode::__registerPokeGlobalsOnce()
{
	__registerGlobalsOnce(_pokeLuaState, _pokeBaseEnvRef, _pokeGlobalsRegistered);
}

void ScriptNode::__registerGlobalsOnce(lua_State* luaState, int baseEnvRef, bool& registered)
{
	if(registered) return;

	// Save whatever is currently live as globals (e.g. globals just written by the session ahead of this
	// run), so those writes aren't lost by temporarily swapping globals to the base table below.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [currentEnv]
	int currentEnvRef = luaL_ref(luaState, LUA_REGISTRYINDEX); // [ ]

	// Temporarily make the permanent base table live as globals, so _registerCoreGlobals()'s
	// lua_setglobal() calls land there instead of the current live env table, and so they survive
	// every future __installFreshEnv() reset.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, baseEnvRef); // [base]
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [ ]

	_registerCoreGlobals(luaState);

	// Restore the env that was live before; its metatable already falls through to the base table, so it
	// picks up the newly registered bindings the same way it already picks up stdlib functions.
	lua_rawgeti(luaState, LUA_REGISTRYINDEX, currentEnvRef); // [currentEnv]
	lua_rawseti(luaState, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS); // [ ]
	luaL_unref(luaState, LUA_REGISTRYINDEX, currentEnvRef);

	registered = true;
}

void ScriptNode::_registerCoreGlobals(lua_State* luaState)
{
	__registerTriggerBindings(luaState);
}

void ScriptNode::__registerTriggerBindings(lua_State* luaState)
{
	// Expose GraphNode::Type as a global table of integer constants that round-trip through
	// __checkNodeType(); the values must match static_cast<lua_Integer> of each enum member.
	const std::vector<std::string> nodeTypeNames = GraphNode::typeNames();

	lua_createtable(luaState, 0, static_cast<int>(nodeTypeNames.size()));

	for(const std::string& name : nodeTypeNames)
	{
		lua_pushinteger(luaState, static_cast<lua_Integer>(GraphNode::typeFromName(name)));
		lua_setfield(luaState, -2, name.c_str());
	}

	lua_setglobal(luaState, "NodeType");

	lua_pushlightuserdata(luaState, this);
	lua_pushcclosure(luaState, __luaTrigger, 1);
	lua_setglobal(luaState, "trigger");
}

int ScriptNode::__luaTrigger(lua_State* luaState)
{
	ScriptNode* node = static_cast<ScriptNode*>(lua_touserdata(luaState, lua_upvalueindex(1)));

	// An empty name is what TriggerAction takes as "any name", so an absent or nil first argument needs no
	// special handling here.
	const char* nodeName = luaL_optstring(luaState, 1, "");

	// A type, on the other hand, has no such "any" value: GRAPH_NODE is a type in its own right, so whether
	// to restrict by type at all has to be carried separately.
	bool restrictToNodeType = !lua_isnoneornil(luaState, 2);

	GraphNode::Type nodeType = restrictToNodeType ?
		__checkNodeType(luaState, 2) : GraphNode::Type::GRAPH_NODE;

	Handle<GraphNode> handle(node);

	// Action will self delete once complete.
	TriggerAction* action = new TriggerAction(handle, nodeName, restrictToNodeType, nodeType);

	action -> incrRef();

	node -> _emitAction(action);

	action -> decrRef();

	return 0;
}

GraphNode::Type ScriptNode::__checkNodeType(lua_State* luaState, int index)
{
	lua_Integer value = luaL_checkinteger(luaState, index);

	for(const std::string& name : GraphNode::typeNames())
	{
		GraphNode::Type type = GraphNode::typeFromName(name);

		if(static_cast<lua_Integer>(type) == value) return type;
	}

	// luaL_error does not return; the return below is only present to satisfy the compiler.
	luaL_error(luaState, "invalid NodeType value: %d", static_cast<int>(value));
	return GraphNode::Type::GRAPH_NODE;
}

const char* ScriptNode::__pokeTypeName(GraphPoke::PokeType type)
{
	if(type == GraphPoke::PokeType::GRAB) return "GRAB";
	if(type == GraphPoke::PokeType::DRAG) return "DRAG";
	if(type == GraphPoke::PokeType::HOVER_ENTER) return "HOVER_ENTER";
	if(type == GraphPoke::PokeType::HOVER_LEAVE) return "HOVER_LEAVE";

	return "HIT";
}

void* ScriptNode::__alloc(void* userData, void* ptr, size_t oldSize, size_t newSize)
{
	size_t* memoryUsed = static_cast<size_t*>(userData);

	if(newSize == 0)
	{
		if(ptr) *memoryUsed -= oldSize;
		free(ptr);
		return 0;
	}

	size_t usedWithoutThisBlock = *memoryUsed - (ptr ? oldSize : 0);

	if(usedWithoutThisBlock + newSize > MEMORY_LIMIT) return 0;

	void* newPtr = realloc(ptr, newSize);

	if(newPtr) *memoryUsed = usedWithoutThisBlock + newSize;

	return newPtr;
}

int ScriptNode::__safeLoad(lua_State* luaState)
{
	size_t len = 0;
	const char* chunk = luaL_checklstring(luaState, 1, &len);
	const char* chunkName = luaL_optstring(luaState, 2, chunk);

	// Mode "t" refuses precompiled bytecode chunks, which could otherwise be used to crash or escape the VM.
	if(luaL_loadbufferx(luaState, chunk, len, chunkName, "t") != LUA_OK)
	{
		lua_pushnil(luaState);
		lua_insert(luaState, -2);
		return 2;
	}

	return 1;
}

void ScriptNode::_readDoubleArray(lua_State* luaState, int tableIndex, const char* field, double* out, int count)
{
	lua_getfield(luaState, tableIndex, field); // [..., field]

	if(lua_istable(luaState, -1))
	{
		for(int i = 0; i < count; i++)
		{
			lua_geti(luaState, -1, i + 1); // [..., field, element]
			out[i] = lua_tonumber(luaState, -1);
			lua_pop(luaState, 1); // [..., field]
		}
	}

	lua_pop(luaState, 1); // [...]
}

void ScriptNode::_readByteArray(lua_State* luaState, int tableIndex, const char* field, std::byte* out, int count)
{
	lua_getfield(luaState, tableIndex, field); // [..., field]

	if(lua_istable(luaState, -1))
	{
		for(int i = 0; i < count; i++)
		{
			lua_geti(luaState, -1, i + 1); // [..., field, element]

			// lua_tointeger() only succeeds on values that are already exactly integral, returning 0 for
			// any other number (e.g. the fractional result of a colour lerp), so round instead.
			out[i] = static_cast<std::byte>(std::lround(lua_tonumber(luaState, -1)));

			lua_pop(luaState, 1); // [..., field]
		}
	}

	lua_pop(luaState, 1); // [...]
}
