#ifndef STROBE_SCRIPT_NODE_H
#define STROBE_SCRIPT_NODE_H

#include <atomic>
#include <string>

#include "../actionTargets/StrobeActionTarget.hpp"
#include "ScriptNode.hpp"

/**
 * Graph node that combines ScriptNode's Lua scripting with strobe action support.
 */
class StrobeScriptNode : public ScriptNode, public StrobeActionTarget
{
    public:

        virtual ~StrobeScriptNode();

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 */
        StrobeScriptNode(const std::string& coreScript, const std::string& pokeScript);

		void setStrobe(bool flag) override;

		StrobeActionTarget* getStrobeActionTarget() override;

	protected:

		void _registerCoreGlobals(lua_State* luaState) override;

	private:

        // Do not allow copying.
        StrobeScriptNode(const StrobeScriptNode& copyFrom);
        StrobeScriptNode& operator= (const StrobeScriptNode& copyFrom);

		/**
		 * Lua-facing `getStrobe()`: returns whether the node bound as this closure's upvalue is currently
		 * marked as strobing.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target StrobeScriptNode.
		 * @returns Always 1 (the strobe flag is left on the stack).
		 */
		static int __luaGetStrobe(lua_State* luaState);

		/**
		 * Register the `getStrobe()` global function against luaState, bound to this node instance via
		 * upvalue.
		 * @param luaState Lua state to register the global against.
		 */
		void __registerStrobeBindings(lua_State* luaState);

		/// Flag to indicate if this node is currently marked as strobing.
		std::atomic<bool> _strobe = false;
};

#endif
