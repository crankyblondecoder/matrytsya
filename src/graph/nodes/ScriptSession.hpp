#ifndef SCRIPT_SESSION_H
#define SCRIPT_SESSION_H

#include "../../util/Handle.hpp"
#include "../../util/RefCounted.hpp"

class ScriptNode;

struct lua_State;

/**
 * Exclusive, ref counted access to one of a ScriptNode's Lua states, obtained from that node via
 * requestCoreSession()/requestPokeSession(). The state's resource lock is claimed by the request and released
 * when the last reference to the session goes away, so staging globals, running the script and reading the
 * results back are a single critical section rather than a sequence of independently locked calls.
 * @note This is the only way to read or write a global on a ScriptNode. Nothing outside a session can reach
 *       either Lua state.
 * @note A session must be released on the thread that requested it. A ThreadResourceLock records ownership per
 *       thread, so unlocking from another thread fails. Do not store a session handle beyond the scope that
 *       requested it, and do not hand one to another thread.
 * @note A thread may not hold two sessions on the same state at once. The second request throws
 *       ThreadException::RESOURCE_LOCK_RE_ENTRY rather than deadlocking. Sessions on the core and poke states
 *       are independent and may be held together.
 */
class ScriptSession : public RefCounted
{
	public:

		/**
		 * Run this session's script - the core script for a core session, the poke script for a poke one -
		 * against its state, with whatever globals have been staged so far already visible to it.
		 * @returns True if the script ran successfully. False if it failed at runtime, or if it never
		 *          compiled at all when the node was constructed.
		 * @note A core script may define init() and invoke() entry points, which changes what a run actually
		 *       executes; see ScriptNode's class documentation. A poke script is always run in full.
		 */
		bool run();

		/**
		 * Set a global in this session's environment, so a script run against it can see it.
		 * @param name Global name to set.
		 * @param value Boolean value to set it to.
		 */
		void setGlobal(const char* name, bool value);

		/**
		 * Set a global in this session's environment, so a script run against it can see it.
		 * @param name Global name to set.
		 * @param value Integer value to set it to.
		 */
		void setGlobal(const char* name, int value);

		/**
		 * Set a global in this session's environment, so a script run against it can see it.
		 * @param name Global name to set.
		 * @param value Floating point value to set it to.
		 */
		void setGlobal(const char* name, double value);

		/**
		 * Set a global in this session's environment, so a script run against it can see it.
		 * @param name Global name to set.
		 * @param value String value to set it to.
		 */
		void setGlobal(const char* name, const char* value);

		/**
		 * Set a global in this session's environment to an array of numbers, presented to the script as a Lua
		 * table indexed from 1.
		 * @param name Global name to set.
		 * @param values Values to populate the table with.
		 * @param count Number of elements in values.
		 */
		void setGlobal(const char* name, const float* values, int count);

		/**
		 * Read a global out of this session's environment as it currently stands, i.e. as last left by run()
		 * or setGlobal(), whichever ran most recently, or as the previous session left it if neither has.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a boolean by that name was found.
		 */
		bool getGlobal(const char* name, bool& value);

		/**
		 * Read a global out of this session's environment as it currently stands, i.e. as last left by run()
		 * or setGlobal(), whichever ran most recently, or as the previous session left it if neither has.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether an integer by that name was found.
		 */
		bool getGlobal(const char* name, int& value);

		/**
		 * Read a global out of this session's environment as it currently stands, i.e. as last left by run()
		 * or setGlobal(), whichever ran most recently, or as the previous session left it if neither has.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a number by that name was found.
		 */
		bool getGlobal(const char* name, double& value);

		/**
		 * Read a global out of this session's environment as it currently stands, i.e. as last left by run()
		 * or setGlobal(), whichever ran most recently, or as the previous session left it if neither has.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a string by that name was found.
		 * @note The returned pointer is anchored by the environment table rather than by this session, so it
		 *       outlives the session and remains valid until that name is next overwritten.
		 */
		bool getGlobal(const char* name, const char*& value);

	protected:

		// Ref counted.
		virtual ~ScriptSession();

	private:

		// Only the node that owns the state hands out sessions against it.
		friend class ScriptNode;

		/**
		 * @param node Node whose state this session drives. Its resource lock for that state must already
		 *        have been claimed by the calling thread.
		 * @param luaState The node's core or poke state, matching poke.
		 * @param poke True if this is a session against the node's poke state, false for its core state.
		 */
		ScriptSession(ScriptNode* node, lua_State* luaState, bool poke);

		// Do not allow copying.
		ScriptSession(const ScriptSession& copyFrom);
		ScriptSession& operator= (const ScriptSession& copyFrom);

		/// Node whose state this session drives. Held as a handle so the state cannot be closed underneath a
		/// live session.
		Handle<ScriptNode> _node;

		/// The node state this session holds the resource lock on.
		lua_State* _luaState = 0;

		/// True if this session is against its node's poke state, false for its core state.
		bool _poke = false;
};

#endif
