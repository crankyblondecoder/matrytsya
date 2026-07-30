#ifndef SCRIPT_ACTION_TARGET_H
#define SCRIPT_ACTION_TARGET_H

#include "ActionTarget.hpp"

#include "../nodes/ScriptSession.hpp"
#include "../../util/Handle.hpp"

/**
 * Action target to use for invoking a node's script.
 */
class ScriptActionTarget : virtual public ActionTarget
{
    public:

        virtual ~ScriptActionTarget() {}

		ScriptActionTarget() {}

		/**
		 * Request exclusive access to this target's core script state, for running that script and for
		 * setting and reading its globals. Nothing else can reach the state, so this is the only way to
		 * invoke the target's script.
		 * @returns Handle to the session. Access to the state lasts only as long as the last reference to it.
		 * @throw ThreadException If the calling thread already holds a session on the core state.
		 * @note Blocks until any session another thread holds on the core state has been released.
		 * @note The caller must hold a reference to this target across the call and for as long as it waits.
		 */
		virtual Handle<ScriptSession> requestCoreSession() = 0;

	protected:

    private:
};

#endif
