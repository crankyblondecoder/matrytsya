#ifndef SCRIPT_ACTION_H
#define SCRIPT_ACTION_H

#include <cstddef>

#include "../GraphAction.hpp"

struct lua_State;

/**
 * Graph action that carries an isolated Lua state through the graph, invoking each visited node's script
 * against it.
 * @note The Lua state opens only the base, coroutine, math, string, table and utf8 libraries. io, os, package
 *       and debug are never opened, so the state has no filesystem, process, environment or introspection
 *       access. Its memory is drawn from an allocator private to this action and capped at MEMORY_LIMIT, so
 *       the only resources available to the script are those this action explicitly grants it.
 * @note Each node visited is given its own fresh global environment, so one node's script can neither see nor
 *       overwrite globals set by another node's script. The only way state crosses from one node to the next
 *       is via _shareGlobal(), which a subclass must call explicitly.
 */
class ScriptAction : public GraphAction
{
    public:

        virtual ~ScriptAction();

		ScriptAction(GraphNodeHandle& initNode);

	protected:

		void _apply(GraphNode* target) override;

		void _complete() override;

		/**
		 * Publish a boolean as a global visible to every node this action visits, bypassing the normal
		 * per-node isolation. This is the only mechanism by which a script can see state left behind by
		 * another node.
		 * @param name Global name the value will be visible under.
		 * @param value Boolean value to publish.
		 */
		void _shareGlobal(const char* name, bool value);

		/**
		 * Publish an integer as a global visible to every node this action visits, bypassing the normal
		 * per-node isolation. This is the only mechanism by which a script can see state left behind by
		 * another node.
		 * @param name Global name the value will be visible under.
		 * @param value Integer value to publish.
		 */
		void _shareGlobal(const char* name, int value);

		/**
		 * Publish a floating point number as a global visible to every node this action visits, bypassing
		 * the normal per-node isolation. This is the only mechanism by which a script can see state left
		 * behind by another node.
		 * @param name Global name the value will be visible under.
		 * @param value Double value to publish.
		 */
		void _shareGlobal(const char* name, double value);

		/**
		 * Publish a string as a global visible to every node this action visits, bypassing the normal
		 * per-node isolation. This is the only mechanism by which a script can see state left behind by
		 * another node.
		 * @param name Global name the value will be visible under.
		 * @param value String value to publish.
		 */
		void _shareGlobal(const char* name, const char* value);

		/**
		 * Read a boolean, preferring the value most recently set by a visited node's script and falling
		 * back to a value published with _shareGlobal() if no node's script set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a boolean by that name was found.
		 */
		bool _getGlobal(const char* name, bool& value);

		/**
		 * Read an integer, preferring the value most recently set by a visited node's script and falling
		 * back to a value published with _shareGlobal() if no node's script set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether an integer by that name was found.
		 */
		bool _getGlobal(const char* name, int& value);

		/**
		 * Read a floating point number, preferring the value most recently set by a visited node's script
		 * and falling back to a value published with _shareGlobal() if no node's script set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a number by that name was found.
		 */
		bool _getGlobal(const char* name, double& value);

		/**
		 * Read a string, preferring the value most recently set by a visited node's script and falling
		 * back to a value published with _shareGlobal() if no node's script set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @note The returned pointer is anchored by the table it was read from and remains valid until
		 *       that table is replaced or the same name is overwritten within it.
		 * @returns Whether a string by that name was found.
		 */
		bool _getGlobal(const char* name, const char*& value);

    private:

        // Do not allow copying.
        ScriptAction(const ScriptAction& copyFrom);
        ScriptAction& operator= (const ScriptAction& copyFrom);

		/**
		 * Publish the value currently on top of _luaState as a global, consuming it. Shared implementation
		 * behind every _shareGlobal() overload, each of which pushes its value before calling this.
		 * @param name Global name the value will be visible under.
		 */
		void __shareGlobal(const char* name);

		/**
		 * Push the named global's value onto _luaState, or nil if it was never set. Looks first in the
		 * last visited node's isolated environment table, which falls through automatically (via its
		 * metatable's __index) to the shared table if that node's script never set the name; if no node
		 * has been visited yet, looks directly in the shared table. Shared implementation behind every
		 * _getGlobal() overload, each of which checks the pushed value's type before popping it.
		 * @param name Global name to look up.
		 * @returns Whether the global was found (a non-nil value was pushed).
		 */
		bool __getGlobal(const char* name);

		/**
		 * Replace the state's current global table with a fresh one whose reads fall through, via a
		 * metatable, to the shared table so library functions and anything published with _shareGlobal()
		 * remain visible. Writes made by the node's script land only in the fresh table.
		 */
		void __installIsolatedEnv();

		/**
		 * Save a registry reference to the environment table currently installed as _luaState's globals,
		 * replacing (and unref'ing) whatever _lastNodeEnvRef pointed to previously. Called once a node's
		 * script has finished running so _getGlobal() can read back whatever it set.
		 */
		void __captureLastNodeEnv();

		/**
		 * Allocator given to _luaState. Draws memory from the C heap but refuses any request that would push
		 * this action's usage past MEMORY_LIMIT.
		 * @param userData The ScriptAction instance the state belongs to, as passed to lua_newstate().
		 */
		static void* __alloc(void* userData, void* ptr, size_t oldSize, size_t newSize);

		/**
		 * Replacement for the sandboxed state's global `load`.
		 * @note The real `load` accepts precompiled bytecode chunks by default, which can be used to crash or
		 *       escape the VM. This only accepts source text and always compiles in text-only mode.
		 */
		static int __safeLoad(lua_State* luaState);

		/// Maximum number of bytes _luaState may have allocated at any one time.
		static constexpr size_t MEMORY_LIMIT = 1 * 1024 * 1024;

		/// Isolated Lua state this action carries through the graph as it traverses.
		lua_State* _luaState = 0;

		/**
		 * Registry reference to the table backing every node's environment via metatable __index fall
		 * through. Holds the sandbox's libraries plus anything published with _shareGlobal(). Always
		 * assigned in the constructor before any node is visited.
		 */
		int _sharedEnvRef = 0;

		/**
		 * Registry reference to the isolated environment table of the last node visited by _apply(), or 0
		 * if no node has been visited yet. _getGlobal() reads through this table first so it sees values
		 * set directly by that node's script; the table's metatable falls through to the shared table if
		 * the script never set the requested name.
		 */
		int _lastNodeEnvRef = 0;

		/// Running total of bytes currently allocated by _luaState via __alloc.
		size_t _memoryUsed = 0;
};

#endif
