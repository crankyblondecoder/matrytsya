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
		 * Replace the state's current global table with a fresh one whose reads fall through, via a
		 * metatable, to the shared table so library functions and anything published with _shareGlobal()
		 * remain visible. Writes made by the node's script land only in the fresh table.
		 */
		void __installIsolatedEnv();

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

		/// Running total of bytes currently allocated by _luaState via __alloc.
		size_t _memoryUsed = 0;
};

#endif
