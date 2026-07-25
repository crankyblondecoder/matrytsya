#ifndef MODEL_TOOL_BINDING_H
#define MODEL_TOOL_BINDING_H

#include <string>
#include <vector>

#include "../util/RefCounted.hpp"

class ModelToolDefinition;
class ModelToolCallParameterValue;

/**
 * Binding of the concrete implementation of a tools to their definitions.
 */
class ModelToolBindings : public RefCounted
{
	public:

		/**
		 * Get the definitions of the model tools.
		 */
		virtual std::vector<ModelToolDefinition> getModelToolDefinitions() = 0;

		/**
		 * Determine if the tool call binding with the given name is available.
		 * @param name Name of the binding to look for.
		 * @returns True if the binding is available.
		 * @note The default implementation looks up names registered with _registerBinding().
		 */
		virtual bool hasBinding(std::string name);

		/**
		 * Process the tool call binding with the given name.
		 * @param name Name of the binding to process.
		 * @param parameterValues Values supplied for the parameters of the tool call.
		 * @returns The result of the tool call.
		 */
		virtual ModelToolCallParameterValue processBinding( std::string name,
			std::vector<ModelToolCallParameterValue> parameterValues) = 0;

	protected:

		ModelToolBindings() {}

		virtual ~ModelToolBindings() {}

		/**
		 * Register the name of a binding this class exposes, so that it is found by the default
		 * hasBinding() implementation.
		 * @param name Name of the binding to register.
		 */
		void _registerBinding(std::string name);

		/**
		 * Find the value supplied for a named parameter of a tool call.
		 * @param parameterValues Values supplied for the parameters of the tool call.
		 * @param parameterName Name of the parameter to find the value of.
		 * @returns The value found.
		 * @throw AgentException When no value for that parameter was supplied.
		 */
		ModelToolCallParameterValue& _getParameterValue(
			std::vector<ModelToolCallParameterValue>& parameterValues, std::string parameterName);

	private:

		// Disable copying.
		ModelToolBindings(const ModelToolBindings& copyFrom);
		ModelToolBindings& operator= (const ModelToolBindings& copyFrom);

		/// Names of the bindings registered via _registerBinding().
		std::vector<std::string> _bindingNames;
};

#endif
