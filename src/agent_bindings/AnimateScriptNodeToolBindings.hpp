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
		 */
		AnimateScriptNodeToolBindings(Handle<AnimateScriptNode> node);

		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() override;

		virtual ModelToolCallParameterValue processBinding(std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) override;

		/**
		 * Get whether the bound node is currently in animating mode.
		 */
		bool getAnimating();

		/**
		 * Set whether the bound node is in animating mode.
		 * @param animating True to mark the node as animating, false otherwise.
		 */
		void setAnimating(bool animating);

	protected:

		virtual ~AnimateScriptNodeToolBindings(){}

	private:

		// Disable copying.
		AnimateScriptNodeToolBindings(const AnimateScriptNodeToolBindings& copyFrom);
		AnimateScriptNodeToolBindings& operator= (const AnimateScriptNodeToolBindings& copyFrom);

		/// Node the bindings operate against.
		Handle<AnimateScriptNode> _node;
};

#endif
