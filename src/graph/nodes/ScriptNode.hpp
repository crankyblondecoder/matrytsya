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

    private:

        // Do not allow copying.
        ScriptNode(const ScriptNode& copyFrom);
        ScriptNode& operator= (const ScriptNode& copyFrom);

		/// Lua source that is run each time this node is invoked.
		std::string _script;
};

#endif
