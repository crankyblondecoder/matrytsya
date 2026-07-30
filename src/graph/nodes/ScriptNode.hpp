#ifndef SCRIPT_NODE_H
#define SCRIPT_NODE_H

#include <cstddef>
#include <string>

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../GraphSerialisedActionNode.hpp"
#include "../../thread/ThreadResourceLock.hpp"

struct lua_State;

/**
 * Graph node that owns its own isolated, sandboxed Lua states, one for its core script and one for
 * processing pokes, and runs its scripts against them when invoked/poked.
 * @note This class is only intended to be inherited and not directly part of the graph.
 * @note Each state opens only the base, coroutine, math, string, table and utf8 libraries. io, os, package
 *       and debug are never opened, so neither state has filesystem, process, environment or introspection
 *       access. Each state's memory is drawn from an allocator private to it and independently capped, so
 *       the only resources available to either script are those explicitly granted to it.
 * @note Neither state is reachable except through a ScriptSession requested from this node, so that is also
 *       the only way to set or read a global on either of them.
 * @note Each state's global environment persists across every run: a global a script sets during one run is
 *       still visible as its own starting state on the next run of the same script on the same node, so a
 *       script can keep state (e.g. a running counter or a direction flag) in an ordinary global instead of
 *       smuggling it through some other channel.
 * @note Extra globals a subclass registers via _registerCoreGlobals() (e.g. getStrobe(), addVertex()) are
 *       written into each state's permanent base table once - the first time a core session runs a script,
 *       the first time a poke happens for the poke state - and remain callable on every subsequent run
 *       without being re-registered, the same as any other global the script has already set.
 * @note A Lua state cannot be driven by more than one thread at a time, and actions are applied to a node
 *       simultaneously, so each state has its own resource lock. A session claims it on request and holds it
 *       for the session's whole life - staged globals, the script run itself and anything read back
 *       afterwards - rather than for one call at a time. The two states lock independently, so a poke never
 *       waits on a core session.
 */
class ScriptNode : public GraphSerialisedActionNode, public ScriptActionTarget
{
    public:

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 */
        ScriptNode(const std::string& coreScript, const std::string& pokeScript);

		Type getType() override;

		Handle<ScriptSession> requestCoreSession() override;

		/**
		 * Request exclusive access to this node's poke state, for running its poke script and for setting and
		 * reading that state's globals. Analogous to requestCoreSession(), but for the poke script's own
		 * state rather than the core script's.
		 * @returns Handle to the session. Access to the state lasts only as long as the last reference to it.
		 * @throw ThreadException If the calling thread already holds a session on the poke state.
		 * @throw GraphException::SCRIPT_SESSION_NODE_UNAVAILABLE If this node could not be referenced.
		 * @note Blocks until any session another thread holds on the poke state has been released.
		 * @note The caller must hold a reference to this node across the call and for as long as it waits, as
		 *       the resource lock it waits on cannot be destructed while a thread is waiting on it.
		 */
		Handle<ScriptSession> requestPokeSession();

		ScriptActionTarget* getScriptActionTarget() override;

	protected:

		// Ref counted.
        virtual ~ScriptNode();

		/**
		 * Hook called once per Lua state this node owns - once the first time a core session runs a script
		 * against the core state, and once the first time this node is poked, against the poke state -
		 * with that state's live global table temporarily pointed at its permanent base env table rather
		 * than the live one. Subclasses override this to register extra globals (typically C closures
		 * bound to this node instance via upvalue) that must remain callable on every future run without
		 * being re-registered each time.
		 * @param luaState The Lua state (core or poke) being registered against, positioned with an empty
		 *        stack and its live global table temporarily set to that state's permanent base env table.
		 * @note This implementation registers the NodeType constants and trigger() binding, so an
		 *       overriding subclass must call it, otherwise neither is callable from that subclass's
		 *       scripts.
		 */
		virtual void _registerCoreGlobals(lua_State* luaState);

		/**
		 * Read an optional array field out of the table at the given stack index into a fixed-size double
		 * array, leaving entries at their existing values if the field is absent.
		 * @param luaState Lua state to read from.
		 * @param tableIndex Stack index of the table to read the field from.
		 * @param field Name of the field to read.
		 * @param out Array to write the values into.
		 * @param count Number of elements to read.
		 */
		static void _readDoubleArray(lua_State* luaState, int tableIndex, const char* field, double* out, int count);

		/**
		 * Read an optional array field out of the table at the given stack index into a fixed-size byte
		 * array, leaving entries at their existing values if the field is absent.
		 * @param luaState Lua state to read from.
		 * @param tableIndex Stack index of the table to read the field from.
		 * @param field Name of the field to read.
		 * @param out Array to write the values into.
		 * @param count Number of elements to read.
		 */
		static void _readByteArray(lua_State* luaState, int tableIndex, const char* field, std::byte* out, int count);

		void _poked(GraphPoke poke) override;

    private:

		// A session is the only thing permitted to drive either of this node's states, so it is the only
		// thing given access to them.
		friend class ScriptSession;

        // Do not allow copying.
        ScriptNode(const ScriptNode& copyFrom);
        ScriptNode& operator= (const ScriptNode& copyFrom);

		/**
		 * Run the core script against the core state.
		 * @returns True if the script ran successfully. False if it never compiled or failed at runtime.
		 * @note The calling thread must already hold this node's core state lock, i.e. this is only ever
		 *       reached through a core ScriptSession.
		 */
		bool __runCore();

		/**
		 * Run the poke script against the poke state.
		 * @returns True if the script ran successfully. False if it never compiled or failed at runtime.
		 * @note The calling thread must already hold this node's poke state lock, i.e. this is only ever
		 *       reached through a poke ScriptSession.
		 */
		bool __runPoke();

		/**
		 * Release the resource lock on one of this node's states, ending the session that held it.
		 * @param poke True to release the poke state, false to release the core state.
		 * @note Called from ScriptSession's destructor, so a failure to unlock is logged rather than thrown.
		 */
		void __releaseState(bool poke);

		/**
		 * Compile _coreScript once and cache the result in _coreBytecode, so a core session never has to
		 * re-parse the source text. _coreScript never changes after construction, so this only needs
		 * to run once.
		 */
		void __compileCoreScript();

		/**
		 * Compile _pokeScript once and cache the result in _pokeBytecode, mirroring __compileCoreScript().
		 * _pokeScript never changes after construction, so this only needs to run once.
		 */
		void __compilePokeScript();

		/**
		 * lua_Writer callback passed to lua_dump(); appends each chunk of bytecode it produces to the
		 * std::string pointed to by userData.
		 * @param luaState Lua state performing the dump.
		 * @param data Chunk of bytecode to append.
		 * @param size Number of bytes in data.
		 * @param userData Pointer to the std::string being written to.
		 * @returns 0, as required by lua_Writer to continue the dump.
		 */
		static int __writeBytecode(lua_State* luaState, const void* data, size_t size, void* userData);

		/**
		 * Create a new Lua state, sandboxed identically to the other state this node owns: opens only the
		 * base, coroutine, math, string, table and utf8 libraries, strips dofile/loadfile/print/warn, and
		 * installs the safe __safeLoad() replacement for the global `load`. Saves a registry ref to the
		 * resulting clean global table into *baseEnvRef.
		 * @param memoryUsed Byte counter __alloc() tracks this state's allocations against and caps at
		 *        MEMORY_LIMIT.
		 * @param baseEnvRef Out-param: registry ref (in the new state) to the clean sandboxed base env
		 *        table.
		 * @returns The newly created and sandboxed state.
		 * @throw GraphException::SCRIPT_STATE_BAD_ALLOC If the state could not be allocated.
		 */
		static lua_State* __createSandboxedState(size_t* memoryUsed, int* baseEnvRef);

		/**
		 * Install a fresh table as luaState's live global table, with its __index metamethod falling
		 * through to the table referenced by baseEnvRef, so library functions remain visible but no write
		 * lands in the base table. Called once per state, at construction, to seed the persistent
		 * environment that state's sessions run against and write into from then on.
		 * @param luaState State to install the fresh environment into.
		 * @param baseEnvRef Registry ref (in luaState) to fall through reads to.
		 */
		static void __installFreshEnv(lua_State* luaState, int baseEnvRef);

		/**
		 * Get the printable name of a poke type, as exposed to a poke script as POKE_TYPE.
		 * @param type Poke type to name.
		 * @returns The name of that type.
		 */
		static const char* __pokeTypeName(GraphPoke::PokeType type);

		/**
		 * Register this node's core Lua bindings exactly once, the first time it is needed, writing them
		 * into the permanent base env table (_coreBaseEnvRef) instead of the persistent live env table so
		 * they remain callable even if a script overwrites a global of the same name in its own env. Does
		 * nothing on any call after the first.
		 */
		void __registerCoreGlobalsOnce();

		/**
		 * Register this node's Lua bindings against the poke state exactly once, the first time this node
		 * is poked, mirroring __registerCoreGlobalsOnce() but against _pokeLuaState/_pokeBaseEnvRef so a
		 * poke script has access to the same subclass-registered globals (e.g. setAnimating()) as the core
		 * script. Does nothing on any call after the first.
		 */
		void __registerPokeGlobalsOnce();

		/**
		 * Shared implementation behind __registerCoreGlobalsOnce()/__registerPokeGlobalsOnce(): temporarily
		 * points luaState's live global table at baseEnvRef so _registerCoreGlobals()'s lua_setglobal()
		 * calls land there instead of the current live env table, calls _registerCoreGlobals(), then
		 * restores the live global table that was in effect beforehand.
		 * @param luaState Lua state to register against.
		 * @param baseEnvRef Registry ref (in luaState) of the permanent base env table to register into.
		 * @param registered In/out: skipped entirely if already true; set true once registration runs.
		 */
		void __registerGlobalsOnce(lua_State* luaState, int baseEnvRef, bool& registered);

		/**
		 * Register the NodeType constant table and the trigger() binding into luaState, so both the core
		 * and the poke script of every ScriptNode subclass can emit a TriggerAction.
		 * @param luaState Lua state to register into.
		 */
		void __registerTriggerBindings(lua_State* luaState);

		/**
		 * Lua binding: trigger([nodeName], [nodeType]). Emits a TriggerAction from this node, optionally
		 * restricted to nodes of the given name and/or of the given NodeType constant. Both arguments are
		 * optional and either may be nil, in which case that restriction is not applied.
		 * @note The emitted action is never applied to this node itself, so a script cannot trigger the
		 *       node it is running on.
		 * @throw Raises a Lua error if nodeType is present but is not a NodeType constant.
		 */
		static int __luaTrigger(lua_State* luaState);

		/**
		 * Read a NodeType constant off the Lua stack.
		 * @param luaState Lua state to read from.
		 * @param index Stack index of the value to read.
		 * @returns The node type the value names.
		 * @throw Raises a Lua error, which does not return, if the value is not a NodeType constant.
		 */
		static GraphNode::Type __checkNodeType(lua_State* luaState, int index);

		/**
		 * Allocator shared by both persistent Lua states this node owns, capped independently per state via
		 * userData.
		 * @param userData Pointer to the size_t byte counter (either &_coreMemoryUsed or &_pokeMemoryUsed)
		 *        this allocation should be tracked against and capped by MEMORY_LIMIT.
		 */
		static void* __alloc(void* userData, void* ptr, size_t oldSize, size_t newSize);

		/**
		 * Replacement for the sandboxed states' global `load`.
		 * @note The real `load` accepts precompiled bytecode chunks by default, which can be used to crash or
		 *       escape the VM. This only accepts source text and always compiles in text-only mode.
		 */
		static int __safeLoad(lua_State* luaState);

		/// Maximum number of bytes either persistent Lua state this node owns may have allocated at once.
		static constexpr size_t MEMORY_LIMIT = 1 * 1024 * 1024;

		/// Main Lua source that is run each time this node is invoked.
		std::string _coreScript;

		/// Lua source that is exclusively for processing pokes.
		std::string _pokeScript;

		/// Precompiled bytecode of _coreScript, cached once at construction; empty if _coreScript failed
		/// to compile.
		std::string _coreBytecode;

		/// Precompiled bytecode of _pokeScript, cached once at construction; empty if _pokeScript failed
		/// to compile.
		std::string _pokeBytecode;

		/// Claimed for the life of a core session; guards every call into _coreLuaState.
		ThreadResourceLock _coreLock;

		/// Claimed for the life of a poke session; guards every call into _pokeLuaState.
		ThreadResourceLock _pokeLock;

		/// Persistent, sandboxed Lua state this node owns for running its core script.
		lua_State* _coreLuaState = 0;

		/// Persistent, sandboxed Lua state this node owns for running its poke script.
		lua_State* _pokeLuaState = 0;

		/// Registry ref (in _coreLuaState) to the base env table. Reads that miss the persistent live env
		/// table fall through to this one (stdlib functions, etc.); this is also where
		/// _registerCoreGlobals() writes each subclass's C-closure bindings, exactly once, so they are
		/// visible even if a script's own env does not (yet) shadow them with a global of the same name.
		int _coreBaseEnvRef = 0;

		/// Whether _registerCoreGlobals() has already run once for this node instance. Guards
		/// __registerCoreGlobalsOnce() so subclass bindings are installed exactly once, the first time a
		/// core session runs a script, instead of being re-registered on every run.
		bool _coreGlobalsRegistered = false;

		/// Registry ref (in _pokeLuaState) to the clean sandboxed base env table.
		int _pokeBaseEnvRef = 0;

		/// Whether _registerCoreGlobals() has already run once against the poke state for this node
		/// instance. Guards __registerPokeGlobalsOnce() so subclass bindings are installed exactly once,
		/// the first time this node is poked, instead of being re-registered on every poke.
		bool _pokeGlobalsRegistered = false;

		/// Running total of bytes currently allocated by _coreLuaState via __alloc.
		size_t _coreMemoryUsed = 0;

		/// Running total of bytes currently allocated by _pokeLuaState via __alloc.
		size_t _pokeMemoryUsed = 0;
};

#endif
