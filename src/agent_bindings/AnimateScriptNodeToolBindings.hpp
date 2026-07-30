#ifndef ANIMATE_SCRIPT_NODE_TOOL_BINDINGS_H
#define ANIMATE_SCRIPT_NODE_TOOL_BINDINGS_H

#include <string>
#include <vector>

#include "../agent/ModelToolBindings.hpp"
#include "../util/Handle.hpp"

class AnimateScriptNode;
class ModelToolDefinition;
class ModelToolCallParameterValue;

/**
 * Tool bindings that expose an AnimateScriptNode's animating flag to an AI model.
 * @note The tools this exposes do not vary by model capability, so this can be assigned against any
 *       capability.
 */
class AnimateScriptNodeToolBindings : public ModelToolBindings
{
	public:

		/**
		 * Create the animate script node tool bindings.
		 * @param node Node the bindings operate against.
		 * @param serial Serial number passed to the node's setAnimating() whenever this binding sets the
		 *        animating flag.
		 */
		AnimateScriptNodeToolBindings(Handle<AnimateScriptNode> node, unsigned serial);

		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() override;

		virtual ModelToolCallParameterValue processBinding(std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) override;

	protected:

		virtual ~AnimateScriptNodeToolBindings(){}

	private:

		// Disable copying.
		AnimateScriptNodeToolBindings(const AnimateScriptNodeToolBindings& copyFrom);
		AnimateScriptNodeToolBindings& operator= (const AnimateScriptNodeToolBindings& copyFrom);

		/**
		 * Get whether the bound node is currently in animating mode.
		 */
		bool __getAnimating();

		/**
		 * Set whether the bound node is in animating mode.
		 * @param animating True to mark the node as animating, false otherwise.
		 */
		void __setAnimating(bool animating);

		/// Node the bindings operate against.
		Handle<AnimateScriptNode> _node;

		/// Serial number passed to the node's setAnimating() whenever this binding sets the animating flag.
		unsigned _serial;
};

#endif
