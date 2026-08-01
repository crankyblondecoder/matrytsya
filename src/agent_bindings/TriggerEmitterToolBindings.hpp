#ifndef TRIGGER_EMITTER_TOOL_BINDINGS_H
#define TRIGGER_EMITTER_TOOL_BINDINGS_H

#include <string>
#include <vector>

#include "../agent/ModelToolBindings.hpp"
#include "../util/Handle.hpp"

class GraphNode;
class ModelToolDefinition;
class ModelToolCallParameterValue;

/**
 * Tool bindings that let an AI model emit a trigger action from a GraphNode.
 * @note The tools this exposes do not vary by model capability, so this can be assigned against any
 *       capability.
 */
class TriggerEmitterToolBindings : public ModelToolBindings
{
	public:

		/**
		 * Create the graph node tool bindings.
		 * @param node Node the bindings operate against.
		 */
		TriggerEmitterToolBindings(Handle<GraphNode> node);

		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() override;

		virtual ModelToolCallParameterValue processBinding(std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) override;

	protected:

		virtual ~TriggerEmitterToolBindings(){}

	private:

		// Disable copying.
		TriggerEmitterToolBindings(const TriggerEmitterToolBindings& copyFrom);
		TriggerEmitterToolBindings& operator= (const TriggerEmitterToolBindings& copyFrom);

		/**
		 * Find the value supplied for a named parameter of a tool call, without requiring it to be present.
		 * @param parameterValues Values supplied for the parameters of the tool call.
		 * @param parameterName Name of the parameter to find the value of.
		 * @returns Pointer to the value found, or nullptr if none was supplied.
		 */
		ModelToolCallParameterValue* __findParameterValue(
			std::vector<ModelToolCallParameterValue>& parameterValues, std::string parameterName);

		/**
		 * Emit a trigger action from the bound node.
		 * @param nodeName If non-empty, restricts triggering to nodes with this name.
		 * @param nodeType If non-empty, restricts triggering to nodes of this type. Must be one of the names
		 *        this binding declares in its StringChoice for the tool's nodeType parameter.
		 */
		void __emitTrigger(std::string nodeName, std::string nodeType);

		/// Node the bindings operate against.
		Handle<GraphNode> _node;
};

#endif
