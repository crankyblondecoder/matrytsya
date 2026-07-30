#ifndef ANIMATE_SCRIPT_NODE_H
#define ANIMATE_SCRIPT_NODE_H

#include <string>
#include <vector>

#include "../actionTargets/AgentActionTarget.hpp"
#include "../actionTargets/AnimateActionTarget.hpp"
#include "../../util/Handle.hpp"
#include "StrobeScriptNode.hpp"

class ModelToolBindings;

/**
 * Graph node that combines StrobeScriptNode's strobing support with animate and agent action support.
 * @note This class is only intended to be inherited and not directly part of the graph.
 */
class AnimateScriptNode : public StrobeScriptNode, public AnimateActionTarget, public AgentActionTarget
{
    public:

		/**
		 * @param coreScript Main Lua source code that this node runs when invoked.
		 * @param pokeScript The script that is called for processing a poke.
		 */
        AnimateScriptNode(const std::string& coreScript, const std::string& pokeScript);

		// Animate target API point.
		void setAnimating(bool flag, unsigned serial) override;

		/**
		 * Get whether this node is currently in animating mode.
		 */
		bool getAnimating();

		AnimateActionTarget* getAnimateActionTarget() override;

		// Agent target API point.
		std::vector<Handle<ModelToolBindings>> getModelToolBindings(AgenticHarness::Capability capability,
			unsigned serial) override;

		AgentActionTarget* getAgentActionTarget() override;

	protected:

		// Ref counted.
        virtual ~AnimateScriptNode() = 0;

		void _registerCoreGlobals(lua_State* luaState) override;

	private:

        // Do not allow copying.
        AnimateScriptNode(const AnimateScriptNode& copyFrom);
        AnimateScriptNode& operator= (const AnimateScriptNode& copyFrom);

		/**
		 * Lua-facing `getAnimating()`: returns whether the node bound as this closure's upvalue is
		 * currently in animating mode.
		 * @param luaState Lua state the call is running against; upvalue 1 is a light userdata pointing at
		 *        the target AnimateScriptNode.
		 * @returns Always 1 (the animating flag is left on the stack).
		 */
		static int __luaGetAnimating(lua_State* luaState);

		/**
		 * Lua-facing `setAnimating(animating, emitAnimateAction)`: sets whether the node bound as this
		 * closure's upvalue is in animating mode.
		 * @param luaState Lua state the call is running against; argument 1 is the animating boolean,
		 *        argument 2 is an optional boolean (default false) for whether an AnimateAction should be
		 *        emitted if the mode changed, and upvalue 1 is a light userdata pointing at the target
		 *        AnimateScriptNode.
		 * @returns Always 0.
		 */
		static int __luaSetAnimating(lua_State* luaState);

		/**
		 * Register the `getAnimating()`/`setAnimating()` global functions against luaState, binding each to
		 * this node instance via upvalue.
		 * @param luaState Lua state to register the globals against.
		 */
		void __registerAnimatingBindings(lua_State* luaState);

		/**
		 * Get the animating flag state.
		 */
		bool __getAnimating();

		/**
		 * Set whether this node is in animating mode.
		 * @param animating True if in animating mode, false otherwise.
		 * @param serial Serial number of the given animating flag. If zero, serial number comparison is not used.
		 *        If non-zero, the given serial must be greater than the current serial.
		 * @param emitAnimateAction Emit an animate action from this node if the mode changed.
		 */
		void __setAnimating(bool animating, unsigned serial, bool emitAnimateAction);

		/// Generic lock.
		ThreadMutex _lock;

		/// Flag to indicate if this node is currently in animating mode.
		bool _animating = false;

		/// Serial number of the animating flag.
		unsigned _animatingSerial = 0;
};

#endif
