#ifndef SCRIPT_NODE_H
#define SCRIPT_NODE_H

#include <cstddef>
#include <string>

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../GraphNode.hpp"

struct lua_State;

/**
 * Graph node that owns its own isolated, sandboxed Lua states, one for its core script and one for
 * processing pokes, and runs its scripts against them when invoked/poked.
 * @note This class is only intended to be inherited and not directly part of the graph.
 * @note Each state opens only the base, coroutine, math, string, table and utf8 libraries. io, os, package
 *       and debug are never opened, so neither state has filesystem, process, environment or introspection
 *       access. Each state's memory is drawn from an allocator private to it and independently capped, so
 *       the only resources available to either script are those explicitly granted to it.
 * @note Each state's global environment persists across every invoke()/poke: a global a script sets during
 *       one run is still visible as its own starting state on the next run of the same script on the same
 *       node, so a script can keep state (e.g. a running counter or a direction flag) in an ordinary global
 *       instead of smuggling it through some other channel.
 * @note Extra globals a subclass registers via _registerCoreGlobals() (e.g. getStrobe(), addVertex()) are
 *       written into each state's permanent base table once - the first time invoke() runs for the core
 *       state, the first time a poke happens for the poke state - and remain callable on every subsequent
 *       invoke()/poke without being re-registered, the same as any other global the script has already set.
 * @note A Lua state cannot be driven by more than one thread at a time, and actions are applied to a node
 *       simultaneously, so each state has its own lock that is held across every call into it - including
 *       the script run itself. The two states lock independently, so a poke never waits on an invoke().
 */
class ScriptNode : public GraphNode, public ScriptActionTarget
{
    public:

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 */
        ScriptNode(const std::string& coreScript, const std::string& pokeScript);

		Type getType() override;

		bool invoke() override;

		void setGlobal(const char* name, bool value) override;
		void setGlobal(const char* name, int value) override;
		void setGlobal(const char* name, double value) override;
		void setGlobal(const char* name, const char* value) override;

		bool getGlobal(const char* name, bool& value) override;
		bool getGlobal(const char* name, int& value) override;
		bool getGlobal(const char* name, double& value) override;
		bool getGlobal(const char* name, const char*& value) override;

		/**
		 * Read a global out of the poke state's environment as it stood immediately after the last poke (or
		 * as it stood freshly sandboxed, if this node has never been poked). Analogous to getGlobal(), but
		 * for the poke script's own state rather than the core script's.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a boolean by that name was found.
		 */
		bool getPokeGlobal(const char* name, bool& value);

		/**
		 * Read a global out of the poke state's environment as it stood immediately after the last poke (or
		 * as it stood freshly sandboxed, if this node has never been poked). Analogous to getGlobal(), but
		 * for the poke script's own state rather than the core script's.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether an integer by that name was found.
		 */
		bool getPokeGlobal(const char* name, int& value);

		/**
		 * Read a global out of the poke state's environment as it stood immediately after the last poke (or
		 * as it stood freshly sandboxed, if this node has never been poked). Analogous to getGlobal(), but
		 * for the poke script's own state rather than the core script's.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a number by that name was found.
		 */
		bool getPokeGlobal(const char* name, double& value);

		/**
		 * Read a global out of the poke state's environment as it stood immediately after the last poke (or
		 * as it stood freshly sandboxed, if this node has never been poked). Analogous to getGlobal(), but
		 * for the poke script's own state rather than the core script's.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a string by that name was found.
		 */
		bool getPokeGlobal(const char* name, const char*& value);

		ScriptActionTarget* getScriptActionTarget() override;

	protected:

		// Ref counted.
        virtual ~ScriptNode();

		/**
		 * Hook called once per Lua state this node owns - once the first time invoke() runs a script
		 * against the core state, and once the first time this node is poked, against the poke state -
		 * with that state's live global table temporarily pointed at its permanent base env table rather
		 * than a per-invoke one. Subclasses override this to register extra globals (typically C closures
		 * bound to this node instance via upvalue) that must remain callable on every future invoke()/poke
		 * without being re-registered each time. Default implementation does nothing.
		 * @param luaState The Lua state (core or poke) being registered against, positioned with an empty
		 *        stack and its live global table temporarily set to that state's permanent base env table.
		 */
		virtual void _registerCoreGlobals(lua_State* luaState) {}

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

        // Do not allow copying.
        ScriptNode(const ScriptNode& copyFrom);
        ScriptNode& operator= (const ScriptNode& copyFrom);

		/**
		 * Compile _coreScript once and cache the result in _coreBytecode, so invoke() never has to
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
		 * environment that state's invoke()/poke calls run against and write into from then on.
		 * @param luaState State to install the fresh environment into.
		 * @param baseEnvRef Registry ref (in luaState) to fall through reads to.
		 */
		static void __installFreshEnv(lua_State* luaState, int baseEnvRef);

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
		 * calls land there instead of the current per-invoke table, calls _registerCoreGlobals(), then
		 * restores the live global table that was in effect beforehand.
		 * @param luaState Lua state to register against.
		 * @param baseEnvRef Registry ref (in luaState) of the permanent base env table to register into.
		 * @param registered In/out: skipped entirely if already true; set true once registration runs.
		 */
		void __registerGlobalsOnce(lua_State* luaState, int baseEnvRef, bool& registered);

		/**
		 * Push GraphPoke's contents as Lua globals (POKE_TYPE, HIT_DURATION, DRAG_VECTOR) onto luaState,
		 * ahead of running the poke script, so the script can branch on what kind of poke this is.
		 * @param luaState The poke Lua state, positioned with the fresh per-invoke environment already
		 *        installed.
		 * @param poke The poke whose contents should be exposed.
		 */
		static void __exposePokeContext(lua_State* luaState, GraphPoke poke);

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

		/// Guards every call into _coreLuaState.
		ThreadMutex _coreLock;

		/// Guards every call into _pokeLuaState.
		ThreadMutex _pokeLock;

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
		/// __registerCoreGlobalsOnce() so subclass bindings are installed exactly once, the first time
		/// invoke() runs a script, instead of being re-registered on every invoke().
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
