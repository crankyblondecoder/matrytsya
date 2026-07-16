#ifndef SCRIPT_ACTION_TARGET_H
#define SCRIPT_ACTION_TARGET_H

#include "ActionTarget.hpp"

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
		 * @returns True if the script ran successfully.
		 */
		virtual bool invoke() = 0;

		/**
		 * Set a global in this target's own environment before invoke() is called, so the script this
		 * target is about to run can see it.
		 * @param name Global name to set.
		 * @param value Boolean value to set it to.
		 * @note Default implementation is a no-op; only meaningful for targets that own a Lua state.
		 */
		virtual void setGlobal(const char* name, bool value) {}

		/**
		 * Set a global in this target's own environment before invoke() is called, so the script this
		 * target is about to run can see it.
		 * @param name Global name to set.
		 * @param value Integer value to set it to.
		 * @note Default implementation is a no-op; only meaningful for targets that own a Lua state.
		 */
		virtual void setGlobal(const char* name, int value) {}

		/**
		 * Set a global in this target's own environment before invoke() is called, so the script this
		 * target is about to run can see it.
		 * @param name Global name to set.
		 * @param value Floating point value to set it to.
		 * @note Default implementation is a no-op; only meaningful for targets that own a Lua state.
		 */
		virtual void setGlobal(const char* name, double value) {}

		/**
		 * Set a global in this target's own environment before invoke() is called, so the script this
		 * target is about to run can see it.
		 * @param name Global name to set.
		 * @param value String value to set it to.
		 * @note Default implementation is a no-op; only meaningful for targets that own a Lua state.
		 */
		virtual void setGlobal(const char* name, const char* value) {}

		/**
		 * Read a global out of this target's own environment as it currently stands - i.e. as last left by
		 * invoke() or setGlobal(), whichever ran most recently.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a boolean by that name was found.
		 * @note Default implementation always returns false; only meaningful for targets that own a Lua
		 *       state.
		 */
		virtual bool getGlobal(const char* name, bool& value) { return false; }

		/**
		 * Read a global out of this target's own environment as it currently stands - i.e. as last left by
		 * invoke() or setGlobal(), whichever ran most recently.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether an integer by that name was found.
		 * @note Default implementation always returns false; only meaningful for targets that own a Lua
		 *       state.
		 */
		virtual bool getGlobal(const char* name, int& value) { return false; }

		/**
		 * Read a global out of this target's own environment as it currently stands - i.e. as last left by
		 * invoke() or setGlobal(), whichever ran most recently.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a number by that name was found.
		 * @note Default implementation always returns false; only meaningful for targets that own a Lua
		 *       state.
		 */
		virtual bool getGlobal(const char* name, double& value) { return false; }

		/**
		 * Read a global out of this target's own environment as it currently stands - i.e. as last left by
		 * invoke() or setGlobal(), whichever ran most recently.
		 * @param name Global name to look up.
		 * @param value Set to the global's value if found.
		 * @returns Whether a string by that name was found.
		 * @note Default implementation always returns false; only meaningful for targets that own a Lua
		 *       state.
		 */
		virtual bool getGlobal(const char* name, const char*& value) { return false; }

	protected:

    private:
};

#endif
