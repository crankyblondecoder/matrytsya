#ifndef SCRIPT_NODE_H
#define SCRIPT_NODE_H

#include <string>

#include "../actionTargets/ScriptActionTarget.hpp"
#include "../GraphNode.hpp"

struct lua_State;

/**
 * Graph node that runs a script against a Lua state provided to it when invoked.
 */
class ScriptNode : public GraphNode, public ScriptActionTarget
{
    public:

        virtual ~ScriptNode();

		/**
		 * @param script Lua source code that this node runs when invoked.
		 */
        ScriptNode(const std::string& script);

		bool invoke(lua_State* luaState) override;

		ScriptActionTarget* getScriptActionTarget() override;

	protected:

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

    private:

        // Do not allow copying.
        ScriptNode(const ScriptNode& copyFrom);
        ScriptNode& operator= (const ScriptNode& copyFrom);

		/// Lua source that is run each time this node is invoked.
		std::string _script;
};

#endif
