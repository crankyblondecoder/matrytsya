#ifndef SCRIPT_ACTION_H
#define SCRIPT_ACTION_H

#include <map>
#include <string>

#include "../GraphAction.hpp"

class ScriptSession;

/**
 * Graph action that invokes each visited node's script as it traverses the graph.
 * @note Each node owns its own isolated, sandboxed Lua state (see ScriptNode); this action does not own a
 *       Lua state itself. State crosses from one node to the next only via _shareGlobal(), which a
 *       subclass must call explicitly, or via _getGlobal() reading back whatever the last visited node's
 *       own script set.
 */
class ScriptAction : public GraphAction
{
    public:

        virtual ~ScriptAction();

		ScriptAction(Handle<GraphNode>& initNode, unsigned energy = _startingEnergy);

	protected:

		void _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

		/**
		 * Publish a boolean as a global visible to every node this action visits from now on, by pushing it
		 * into each node's own environment just before that node's script runs.
		 * @param name Global name the value will be visible under.
		 * @param value Boolean value to publish.
		 */
		void _shareGlobal(const char* name, bool value);

		/**
		 * Publish an integer as a global visible to every node this action visits from now on, by pushing it
		 * into each node's own environment just before that node's script runs.
		 * @param name Global name the value will be visible under.
		 * @param value Integer value to publish.
		 */
		void _shareGlobal(const char* name, int value);

		/**
		 * Publish a floating point number as a global visible to every node this action visits from now on,
		 * by pushing it into each node's own environment just before that node's script runs.
		 * @param name Global name the value will be visible under.
		 * @param value Double value to publish.
		 */
		void _shareGlobal(const char* name, double value);

		/**
		 * Publish a string as a global visible to every node this action visits from now on, by pushing it
		 * into each node's own environment just before that node's script runs.
		 * @param name Global name the value will be visible under.
		 * @param value String value to publish.
		 */
		void _shareGlobal(const char* name, const char* value);

		/**
		 * Read a boolean, preferring the value most recently set by the last visited node's own script and
		 * falling back to a value published with _shareGlobal() if that node's script never set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a boolean by that name was found.
		 */
		bool _getGlobal(const char* name, bool& value);

		/**
		 * Read an integer, preferring the value most recently set by the last visited node's own script and
		 * falling back to a value published with _shareGlobal() if that node's script never set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether an integer by that name was found.
		 */
		bool _getGlobal(const char* name, int& value);

		/**
		 * Read a floating point number, preferring the value most recently set by the last visited node's
		 * own script and falling back to a value published with _shareGlobal() if that node's script never
		 * set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a number by that name was found.
		 */
		bool _getGlobal(const char* name, double& value);

		/**
		 * Read a string, preferring the value most recently set by the last visited node's own script and
		 * falling back to a value published with _shareGlobal() if that node's script never set that name.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @note The returned pointer is anchored either by the last visited node's own Lua state or by this
		 *       action's shared value store, and remains valid until that source is next overwritten.
		 * @returns Whether a string by that name was found.
		 */
		bool _getGlobal(const char* name, const char*& value);

    private:

        // Do not allow copying.
        ScriptAction(const ScriptAction& copyFrom);
        ScriptAction& operator= (const ScriptAction& copyFrom);

		/**
		 * Request a session on the last visited node's core state, so a _getGlobal() can read back what that
		 * node's script left behind.
		 * @returns Handle to the session, or an invalid handle if no node has been visited yet, the last
		 *          visited node has no script state, or that state could not be claimed.
		 * @note Never throws. A state that cannot be claimed is reported as an invalid handle, because an
		 *       exception escaping into the action work cycle would strand the action.
		 */
		Handle<ScriptSession> __requestLastVisitedSession();

		/**
		 * Tagged holder for one shared global's value, keyed by name in _sharedGlobals. Exactly one of the
		 * type-specific members is meaningful, selected by type.
		 */
		struct SharedValue
		{
			enum class Type { BOOL, INT, DOUBLE, STRING } type = Type::BOOL;
			bool boolValue = false;
			int intValue = 0;
			double doubleValue = 0;
			std::string stringValue;
		};

		/// Values published via _shareGlobal(), pushed into every node's own environment just before that
		/// node's script runs from the point they're published onward.
		std::map<std::string, SharedValue> _sharedGlobals;

		/**
		 * Handle to the node most recently applied by _apply(), so _getGlobal() can read back whatever that
		 * node's own script set. Invalid if no node has been visited yet.
		 * @note A Handle is used rather than a raw ScriptActionTarget* so this cannot dangle if the
		 *       node is destroyed between being visited and a later _getGlobal() call.
		 */
		Handle<GraphNode> _lastVisitedNode;
};

#endif
