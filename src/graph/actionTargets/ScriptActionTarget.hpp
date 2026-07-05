#ifndef SCRIPT_ACTION_TARGET_H
#define SCRIPT_ACTION_TARGET_H

#include "ActionTarget.hpp"

struct lua_State;

/**
 * Action target to use for invoking a node's script.
 */
class ScriptActionTarget : virtual public ActionTarget
{
    public:

        virtual ~ScriptActionTarget() {}

		ScriptActionTarget() {}

		/**
		 * Invoke the node's script.
		 * @param luaState Lua state to run the script against.
		 * @returns True if the script ran successfully.
		 */
		virtual bool invoke(lua_State* luaState) = 0;

	protected:

    private:
};

#endif
