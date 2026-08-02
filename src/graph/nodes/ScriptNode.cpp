#include "ScriptNode.hpp"

#include "ScriptSession.hpp"
#include "ScriptToolBindings.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphException.hpp"
#include "../GraphPoke.hpp"
#include "../actions/TriggerAction.hpp"

#include "../../agent/AgentException.hpp"
#include "../../agent/ModelToolBindings.hpp"
#include "../../agent/ModelToolDefinition.hpp"
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

	// Every PrimitiveType, paired with the name the ToolType constant table exposes it under. Both the
	// registration of those constants and the reading of one back walk these, so both must be extended
	// whenever PrimitiveType is.
	const ModelToolDefinitionParameter::PrimitiveType TOOL_PRIMITIVE_TYPES[] = {
		ModelToolDefinitionParameter::PrimitiveType::STRING,
		ModelToolDefinitionParameter::PrimitiveType::NUMBER,
		ModelToolDefinitionParameter::PrimitiveType::INTEGER,
		ModelToolDefinitionParameter::PrimitiveType::BOOL};

	const char* const TOOL_PRIMITIVE_TYPE_NAMES[] = {"STRING", "NUMBER", "INTEGER", "BOOL"};

	// -- Translation between the tool call values and Lua, used only by the functions below --

	/**
	 * Read the value at the given stack index as a primitive type, rather than as whatever Lua happens to be
	 * holding there.
	 * @param luaState Lua state to read from.
	 * @param index Absolute stack index of the value to read.
	 * @param type Primitive type to read the value as.
	 * @param value Set to the value read, if it could be read.
	 * @returns True if the value could be read as that type.
	 * @note Leaves the stack as it found it.
	 */
	bool __readPrimitiveValue(lua_State* luaState, int index, ModelToolDefinitionParameter::PrimitiveType type,
		ModelToolCallParameterValue::Value& value)
	{
		switch(type)
		{
			case ModelToolDefinitionParameter::PrimitiveType::STRING:
			{
				// lua_tostring() would convert a number in place, changing what a table being walked holds,
				// so the type is checked rather than relying on the conversion failing.
				if(!lua_isstring(luaState, index) || lua_isnumber(luaState, index)) return false;

				value = std::string(lua_tostring(luaState, index));
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::NUMBER:
			{
				if(!lua_isnumber(luaState, index)) return false;

				value = static_cast<double>(lua_tonumber(luaState, index));
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::INTEGER:
			{
				if(!lua_isnumber(luaState, index)) return false;

				// lua_tointeger() only succeeds on values that are already exactly integral, returning 0 for
				// any other number, so round instead, as the fixed size array readers do.
				value = static_cast<long long>(std::llround(lua_tonumber(luaState, index)));
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::BOOL:
			{
				if(!lua_isboolean(luaState, index)) return false;

				value = static_cast<bool>(lua_toboolean(luaState, index));
				return true;
			}
		}

		return false;
	}

	/**
	 * Read the table at the given stack index as an array of a single primitive type.
	 * @param luaState Lua state to read from.
	 * @param index Absolute stack index of the table to read.
	 * @param elementType Primitive type every element is read as.
	 * @param value Set to the array read, if every element could be read.
	 * @returns True if the value is a table and every element read as elementType.
	 * @note Leaves the stack as it found it, on every path.
	 */
	bool __readArrayValue(lua_State* luaState, int index, ModelToolDefinitionParameter::PrimitiveType elementType,
		ModelToolCallParameterValue::Value& value)
	{
		if(!lua_istable(luaState, index)) return false;

		std::vector<std::string> strings;
		std::vector<double> numbers;
		std::vector<long long> integers;
		std::vector<bool> bools;

		lua_Integer count = luaL_len(luaState, index);

		for(lua_Integer i = 1; i <= count; i++)
		{
			lua_geti(luaState, index, i); // [..., element]

			ModelToolCallParameterValue::Value element;

			bool read = __readPrimitiveValue(luaState, lua_gettop(luaState), elementType, element);

			lua_pop(luaState, 1); // [...]

			// A single bad element makes the whole array something the tool never agreed to accept.
			if(!read) return false;

			if(std::holds_alternative<std::string>(element)) strings.push_back(std::get<std::string>(element));
			else if(std::holds_alternative<double>(element)) numbers.push_back(std::get<double>(element));
			else if(std::holds_alternative<long long>(element)) integers.push_back(std::get<long long>(element));
			else bools.push_back(std::get<bool>(element));
		}

		switch(elementType)
		{
			case ModelToolDefinitionParameter::PrimitiveType::STRING: value = strings; return true;
			case ModelToolDefinitionParameter::PrimitiveType::NUMBER: value = numbers; return true;
			case ModelToolDefinitionParameter::PrimitiveType::INTEGER: value = integers; return true;
			case ModelToolDefinitionParameter::PrimitiveType::BOOL: value = bools; return true;
		}

		return false;
	}

	/**
	 * Read the value at the given stack index as the type a tool definition parameter declares, rather than
	 * as whatever Lua happens to be holding there.
	 * @param luaState Lua state to read from.
	 * @param index Absolute stack index of the value to read.
	 * @param parameter Parameter whose declared type the value is read as.
	 * @param value Set to the value read, if it could be read.
	 * @returns True if the value could be read as the declared type.
	 * @note Leaves the stack as it found it, on every path.
	 */
	bool __readParameterValue(lua_State* luaState, int index, ModelToolDefinitionParameter& parameter,
		ModelToolCallParameterValue::Value& value)
	{
		std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
			ModelToolDefinitionParameter::StringChoice> type = parameter.getType();

		if(std::holds_alternative<ModelToolDefinitionParameter::ArrayType>(type))
		{
			return __readArrayValue(luaState, index,
				std::get<ModelToolDefinitionParameter::ArrayType>(type).elementType, value);
		}

		if(std::holds_alternative<ModelToolDefinitionParameter::StringChoice>(type))
		{
			// Whether the string is one of the choices is the definition's business, not this function's.
			return __readPrimitiveValue(luaState, index,
				ModelToolDefinitionParameter::PrimitiveType::STRING, value);
		}

		return __readPrimitiveValue(luaState, index,
			std::get<ModelToolDefinitionParameter::PrimitiveType>(type), value);
	}

	/**
	 * Push a tool call parameter value onto the stack, as the Lua value matching whichever alternative it
	 * holds. An array valued parameter is pushed as a table indexed from 1, as arrays are elsewhere.
	 * @param luaState Lua state to push onto.
	 * @param value Value to push.
	 */
	void __pushParameterValue(lua_State* luaState, ModelToolCallParameterValue::Value value)
	{
		if(std::holds_alternative<std::string>(value))
		{
			lua_pushstring(luaState, std::get<std::string>(value).c_str());
		}
		else if(std::holds_alternative<double>(value))
		{
			lua_pushnumber(luaState, std::get<double>(value));
		}
		else if(std::holds_alternative<long long>(value))
		{
			lua_pushinteger(luaState, static_cast<lua_Integer>(std::get<long long>(value)));
		}
		else if(std::holds_alternative<bool>(value))
		{
			lua_pushboolean(luaState, std::get<bool>(value));
		}
		else if(std::holds_alternative<std::vector<std::string>>(value))
		{
			const std::vector<std::string>& elements = std::get<std::vector<std::string>>(value);

			lua_createtable(luaState, static_cast<int>(elements.size()), 0);

			for(size_t i = 0; i < elements.size(); i++)
			{
				lua_pushstring(luaState, elements[i].c_str());
				lua_seti(luaState, -2, static_cast<lua_Integer>(i + 1));
			}
		}
		else if(std::holds_alternative<std::vector<double>>(value))
		{
			const std::vector<double>& elements = std::get<std::vector<double>>(value);

			lua_createtable(luaState, static_cast<int>(elements.size()), 0);

			for(size_t i = 0; i < elements.size(); i++)
			{
				lua_pushnumber(luaState, elements[i]);
				lua_seti(luaState, -2, static_cast<lua_Integer>(i + 1));
			}
		}
		else if(std::holds_alternative<std::vector<long long>>(value))
		{
			const std::vector<long long>& elements = std::get<std::vector<long long>>(value);

			lua_createtable(luaState, static_cast<int>(elements.size()), 0);

			for(size_t i = 0; i < elements.size(); i++)
			{
				lua_pushinteger(luaState, static_cast<lua_Integer>(elements[i]));
				lua_seti(luaState, -2, static_cast<lua_Integer>(i + 1));
			}
		}
		else
		{
			const std::vector<bool>& elements = std::get<std::vector<bool>>(value);

			lua_createtable(luaState, static_cast<int>(elements.size()), 0);

			for(size_t i = 0; i < elements.size(); i++)
			{
				lua_pushboolean(luaState, elements[i]);
				lua_seti(luaState, -2, static_cast<lua_Integer>(i + 1));
			}
		}
	}
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
	// Note: The session's lock is deliberately still held across the script run itself. Nothing else may
	// touch the core state while a script is running against it, and a script's callbacks only ever reach
	// back into this node's other state (vertexes, transform, animating flag), never into the core state, so
	// no call made from inside the run can arrive back here.

	// A failed prime leaves whatever init() was setting up half built, so invoke() is held back - for this run
	// only. The next run finds init() already spent and goes straight on to invoke().
	if(!__primeCoreScript()) return false;

	// The environment the script just ran against is left live rather than replaced, so any global it set (or
	// that the session staged ahead of this run) is still there, as the starting state, for the next session.
	return __callOptionalGlobal(_coreLuaState, "invoke");
}

bool ScriptNode::__primeCoreScript()
{
	if(_coreBytecode.empty()) return false;

	__registerCoreGlobalsOnce();

	// The chunk is only re-run while the script has left neither entry point behind. The env starts empty, so
	// the first run always takes this path, and a script that defines neither takes it on every run, which is
	// what makes a script written before either existed behave exactly as it always did. Once either is
	// defined the chunk has already done its job - it built the entry points, and the top level locals they
	// hold as upvalues - so running it again would only throw that work away.
	if(!__globalIsFunction(_coreLuaState, "init") && !__globalIsFunction(_coreLuaState, "invoke"))
	{
		// Mode "b" only accepts bytecode. _coreBytecode is compiled from _coreScript once, at construction, by
		// this class itself rather than supplied by the script being run, so it never crosses the trust
		// boundary that the "t"-only loading elsewhere in this module guards against.
		if(luaL_loadbufferx(_coreLuaState, _coreBytecode.data(), _coreBytecode.size(), "script", "b") != LUA_OK)
		{
			// luaL_loadbufferx leaves an error message on the stack on failure.
			lua_pop(_coreLuaState, 1);
			return false;
		}

		if(lua_pcall(_coreLuaState, 0, 0, 0) != LUA_OK)
		{
			// lua_pcall replaces the function it was given with the error object on failure, so one value is
			// always left behind by a failed call and has to be popped or it accumulates across runs.
			lua_pop(_coreLuaState, 1);

			// A chunk that did not finish cannot be trusted to have defined either entry point, so neither is
			// reached this run, and init()'s one attempt is not spent on a run that never got as far as it.
			return false;
		}
	}

	// init() is offered exactly one run: the first that gets this far. The flag is consumed before the call
	// rather than after it, so an init() that raises is never retried on a later run.
	if(!_coreInitCalled)
	{
		_coreInitCalled = true;
		return __callOptionalGlobal(_coreLuaState, "init");
	}

	return true;
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

		// lua_pcall replaces the function it was given with the error object on failure, so one value is left
		// behind by a failed call and has to be popped or it accumulates on this long lived state.
		if(!success) lua_pop(_pokeLuaState, 1);
	}
	else
	{
		lua_pop(_pokeLuaState, 1);
	}

	// The environment the poke script just ran against is left live rather than replaced, so any global it
	// set is still there, as the starting state, the next time this node is poked.
	return success;
}

bool ScriptNode::__globalIsFunction(lua_State* luaState, const char* name)
{
	bool isFunction = lua_getglobal(luaState, name) == LUA_TFUNCTION; // [value]

	lua_pop(luaState, 1); // [ ]

	return isFunction;
}

bool ScriptNode::__callOptionalGlobal(lua_State* luaState, const char* name)
{
	if(lua_getglobal(luaState, name) != LUA_TFUNCTION) // [value]
	{
		lua_pop(luaState, 1); // [ ]
		return true;
	}

	if(lua_pcall(luaState, 0, 0, 0) == LUA_OK) return true; // [ ]

	// lua_pcall leaves the error object where the function it called was. Nothing reports it, so it is dropped
	// rather than left to accumulate on a state that lives as long as this node does.
	lua_pop(luaState, 1); // [ ]

	return false;
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
	__registerToolTypeConstants(luaState);
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

// -- Script defined tool bindings --

void ScriptNode::__registerToolTypeConstants(lua_State* luaState)
{
	// Exposed as a table of integer constants that round-trip through __readToolParameter(); the values must
	// match static_cast<lua_Integer> of each PrimitiveType member.
	lua_createtable(luaState, 0, static_cast<int>(std::size(TOOL_PRIMITIVE_TYPES)));

	for(size_t i = 0; i < std::size(TOOL_PRIMITIVE_TYPES); i++)
	{
		lua_pushinteger(luaState, static_cast<lua_Integer>(TOOL_PRIMITIVE_TYPES[i]));
		lua_setfield(luaState, -2, TOOL_PRIMITIVE_TYPE_NAMES[i]);
	}

	lua_setglobal(luaState, "ToolType");
}

std::vector<Handle<ModelToolBindings>> ScriptNode::_getScriptToolBindings(AgenticHarness::Capability capability,
	unsigned serial)
{
	std::vector<ModelToolDefinition> definitions;

	try
	{
		{ Handle<ScriptSession> sessionHandle = requestCoreSession();

			// An agent action can reach a node whose core script has never run, so the globals the script
			// declares its tools through have to be brought into existence before one can be looked up.
			if(__primeCoreScript())
			{
				if(lua_getglobal(_coreLuaState, "getToolCallBindings") != LUA_TFUNCTION) // [value]
				{
					lua_pop(_coreLuaState, 1); // [ ]
				}
				else
				{
					// The capability is passed by name so a script can offer a weaker model a smaller set.
					lua_pushstring(_coreLuaState,
						AgenticHarness::getCapabilityName(capability).c_str()); // [function, capability]

					if(lua_pcall(_coreLuaState, 1, 1, 0) == LUA_OK // [descriptors]
						&& lua_istable(_coreLuaState, -1))
					{
						int descriptorsIndex = lua_gettop(_coreLuaState);

						lua_Integer count = luaL_len(_coreLuaState, descriptorsIndex);

						for(lua_Integer i = 1; i <= count; i++)
						{
							lua_geti(_coreLuaState, descriptorsIndex, i); // [descriptors, descriptor]

							// A descriptor that cannot be used is dropped rather than failing the whole set:
							// a tool the model cannot call is worse than a tool it never sees.
							__readToolDefinition(_coreLuaState, lua_gettop(_coreLuaState), definitions);

							lua_pop(_coreLuaState, 1); // [descriptors]
						}
					}

					// Whatever the call left behind - what it returned, or the error object of a call that
					// raised - is popped so it cannot accumulate on a state that lives as long as this node.
					lua_pop(_coreLuaState, 1); // [ ]
				}
			}
		}
	}
	catch(ThreadException& ex)
	{
		// A node whose core state cannot be claimed offers the model no tools, rather than failing the agent
		// action that asked for them.
		LOG(Logger::LogLevel::DEBUG, "Script tool bindings skipped; could not obtain a session on the core state.")

		return std::vector<Handle<ModelToolBindings>>();
	}

	if(definitions.empty()) return std::vector<Handle<ModelToolBindings>>();

	std::vector<Handle<ModelToolBindings>> tools;

	ScriptToolBindings* bindings = new ScriptToolBindings(Handle<ScriptNode>(this), serial, definitions);

	Handle<ModelToolBindings> handle(bindings);

	// The handle carries the reference out to the caller; release the implicit construction ref.
	bindings -> decrRef();

	tools.push_back(handle);

	return tools;
}

ModelToolCallParameterValue ScriptNode::__callScriptTool(const std::string& name, ModelToolDefinition& definition,
	std::vector<ModelToolCallParameterValue>& parameterValues, unsigned serial)
{
	ModelToolDefinitionParameter returnType = definition.getReturnType();

	ModelToolCallParameterValue::Value result;

	bool called = false;

	try
	{
		{ Handle<ScriptSession> sessionHandle = requestCoreSession();

			ScriptSession* session = sessionHandle.getInstance();

			// Staged the same way a poke's contents are, so a tool can tell one call from the next and pass
			// the serial on to anything it drives that compares them.
			session -> setGlobal("TOOL_CALL_SERIAL", static_cast<int>(serial));

			// A tool can only be called on a node whose script already declared it, so the globals are
			// normally in place by now; priming again costs nothing and keeps this independent of that.
			if(__primeCoreScript())
			{
				if(lua_getglobal(_coreLuaState, name.c_str()) != LUA_TFUNCTION) // [value]
				{
					lua_pop(_coreLuaState, 1); // [ ]
				}
				else
				{
					// The model's arguments arrive as one table keyed by parameter name, matching the object
					// it sent them as. A parameter it left out is simply absent from the table.
					lua_createtable(_coreLuaState, 0,
						static_cast<int>(parameterValues.size())); // [function, args]

					for(ModelToolCallParameterValue& parameterValue : parameterValues)
					{
						__pushParameterValue(_coreLuaState, parameterValue.getValue()); // [function, args, value]
						lua_setfield(_coreLuaState, -2, parameterValue.getParameterName().c_str());
					}

					if(lua_pcall(_coreLuaState, 1, 1, 0) == LUA_OK) // [returned]
					{
						if(definition.hasReturnType())
						{
							called = __readParameterValue(_coreLuaState, lua_gettop(_coreLuaState), returnType,
								result);
						}
						else
						{
							// The script declared no result, so its function was never asked for one and
							// whatever it happened to return is not read. Getting here without raising is the
							// whole of what this tool has to report, and the stand-in return type the
							// definition carries is what it is reported under.
							result = true;
							called = true;
						}
					}
					else
					{
						// Nothing else reports this, and the model is only told that the tool failed, so the
						// script author's one sight of why is the log.
						const char* message = lua_tostring(_coreLuaState, -1);

						LOG(Logger::LogLevel::DEBUG, std::string("Script tool '") + name + "' raised: "
							+ (message ? message : "unknown error"))
					}

					// Whatever the call left behind - what it returned, or the error object of a call that
					// raised - is popped so it cannot accumulate on a state that lives as long as this node.
					lua_pop(_coreLuaState, 1); // [ ]
				}
			}
		}
	}
	catch(ThreadException& ex)
	{
		LOG(Logger::LogLevel::DEBUG, std::string("Script tool '") + name
			+ "' failed; could not obtain a session on the core state.")

		throw AgentException(AgentException::SCRIPT_TOOL_FAILED);
	}

	if(!called) throw AgentException(AgentException::SCRIPT_TOOL_FAILED);

	return ModelToolCallParameterValue(returnType.getName(), result);
}

bool ScriptNode::__readToolDefinition(lua_State* luaState, int index,
	std::vector<ModelToolDefinition>& definitions)
{
	if(!lua_istable(luaState, index)) return false;

	lua_getfield(luaState, index, "name"); // [..., name]
	const char* nameValue = lua_tostring(luaState, -1);
	std::string name = nameValue ? nameValue : "";
	lua_pop(luaState, 1); // [...]

	if(name.empty()) return false;

	// The tool is serviced by a global function of the same name, so one that names no function is a tool the
	// model would be offered and could never call.
	if(!__globalIsFunction(luaState, name.c_str())) return false;

	lua_getfield(luaState, index, "description"); // [..., description]
	const char* descriptionValue = lua_tostring(luaState, -1);
	std::string description = descriptionValue ? descriptionValue : "";
	lua_pop(luaState, 1); // [...]

	std::vector<ModelToolDefinitionParameter> parameters;

	lua_getfield(luaState, index, "parameters"); // [..., parameters]

	if(lua_istable(luaState, -1))
	{
		int parametersIndex = lua_gettop(luaState);

		lua_Integer count = luaL_len(luaState, parametersIndex);

		for(lua_Integer i = 1; i <= count; i++)
		{
			lua_geti(luaState, parametersIndex, i); // [..., parameters, parameter]

			std::string parameterName;
			std::string parameterDescription;
			std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
				ModelToolDefinitionParameter::StringChoice> parameterType;
			bool required = true;

			bool read = __readToolParameter(luaState, lua_gettop(luaState), parameterName, parameterDescription,
				parameterType, required);

			lua_pop(luaState, 1); // [..., parameters]

			// A parameter that cannot be read would leave the model calling the tool with arguments the
			// script never agreed to, so the whole tool goes rather than just that parameter.
			if(!read)
			{
				lua_pop(luaState, 1); // [...]
				return false;
			}

			parameters.push_back(ModelToolDefinitionParameter(parameterName, parameterDescription,
				parameterType, required));
		}
	}

	lua_pop(luaState, 1); // [...]

	lua_getfield(luaState, index, "returns"); // [..., returns]

	// No returns field at all declares a tool with no result of its own, rather than a tool described
	// wrongly, so its function is left free to return nothing. A returns that is present but unreadable is a
	// mistake, and still takes the tool with it.
	bool resultDeclared = !lua_isnil(luaState, -1);

	std::string returnName;
	std::string returnDescription;
	std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
		ModelToolDefinitionParameter::StringChoice> returnType;
	bool returnRequired = true;

	bool readReturn = resultDeclared && __readToolParameter(luaState, lua_gettop(luaState), returnName,
		returnDescription, returnType, returnRequired);

	lua_pop(luaState, 1); // [...]

	if(!resultDeclared)
	{
		definitions.push_back(ModelToolDefinition(name, description, parameters));
		return true;
	}

	if(!readReturn) return false;

	definitions.push_back(ModelToolDefinition(name, description, parameters,
		ModelToolDefinitionParameter(returnName, returnDescription, returnType, returnRequired)));

	return true;
}

bool ScriptNode::__readToolParameter(lua_State* luaState, int index, std::string& name, std::string& description,
	std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
	ModelToolDefinitionParameter::StringChoice>& type, bool& required)
{
	if(!lua_istable(luaState, index)) return false;

	lua_getfield(luaState, index, "name"); // [..., name]
	const char* nameValue = lua_tostring(luaState, -1);
	name = nameValue ? nameValue : "";
	lua_pop(luaState, 1); // [...]

	if(name.empty()) return false;

	lua_getfield(luaState, index, "description"); // [..., description]
	const char* descriptionValue = lua_tostring(luaState, -1);
	description = descriptionValue ? descriptionValue : "";
	lua_pop(luaState, 1); // [...]

	lua_getfield(luaState, index, "required"); // [..., required]
	// An absent required field means the model must supply the parameter, matching the default the
	// ModelToolDefinitionParameter constructor applies.
	required = lua_isnil(luaState, -1) || lua_toboolean(luaState, -1);
	lua_pop(luaState, 1); // [...]

	lua_getfield(luaState, index, "type"); // [..., type]
	bool typeIsNumber = lua_isnumber(luaState, -1);
	lua_Integer typeValue = typeIsNumber ? lua_tointeger(luaState, -1) : 0;
	lua_pop(luaState, 1); // [...]

	if(!typeIsNumber) return false;

	ModelToolDefinitionParameter::PrimitiveType primitiveType = ModelToolDefinitionParameter::PrimitiveType::STRING;

	bool typeFound = false;

	for(ModelToolDefinitionParameter::PrimitiveType candidate : TOOL_PRIMITIVE_TYPES)
	{
		if(static_cast<lua_Integer>(candidate) != typeValue) continue;

		primitiveType = candidate;
		typeFound = true;
		break;
	}

	if(!typeFound) return false;

	lua_getfield(luaState, index, "array"); // [..., array]
	bool isArray = lua_toboolean(luaState, -1);
	lua_pop(luaState, 1); // [...]

	if(isArray)
	{
		type = ModelToolDefinitionParameter::ArrayType{primitiveType};
		return true;
	}

	// A restricted set of choices has nowhere to go in the schema of anything but a string, so choices is
	// only honoured for one.
	if(primitiveType == ModelToolDefinitionParameter::PrimitiveType::STRING)
	{
		std::vector<std::string> choices;

		lua_getfield(luaState, index, "choices"); // [..., choices]

		if(lua_istable(luaState, -1))
		{
			int choicesIndex = lua_gettop(luaState);

			lua_Integer count = luaL_len(luaState, choicesIndex);

			for(lua_Integer i = 1; i <= count; i++)
			{
				lua_geti(luaState, choicesIndex, i); // [..., choices, choice]

				const char* choice = lua_tostring(luaState, -1);

				if(choice) choices.push_back(choice);

				lua_pop(luaState, 1); // [..., choices]
			}
		}

		lua_pop(luaState, 1); // [...]

		if(!choices.empty())
		{
			type = ModelToolDefinitionParameter::StringChoice{choices};
			return true;
		}
	}

	type = primitiveType;

	return true;
}
