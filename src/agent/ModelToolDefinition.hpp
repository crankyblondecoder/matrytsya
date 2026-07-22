#ifndef MODEL_TOOL_DEFINITION_H
#define MODEL_TOOL_DEFINITION_H

#include <string>
#include <vector>

#include "ModelToolDefinitionParameter.hpp"

/**
 * Describes the type of a tool call an AI model can make.
 */
class ModelToolDefinition
{
	public:

		/**
		 * Create a description of a tool call.
		 * @param name Name of the tool.
		 * @param description Description of the tool.
		 * @param parameters Parameters the tool accepts.
		 * @param returnType Type of value the tool is expected to return.
		 */
		ModelToolDefinition(std::string name, std::string description,
				std::vector<ModelToolDefinitionParameter> parameters, ModelToolDefinitionParameter returnType);

		/**
		 * Get the name of the tool.
		 */
		std::string getName();

		/**
		 * Get the description of the tool.
		 */
		std::string getDescription();

		/**
		 * Get the parameters the tool call accepts.
		 */
		std::vector<ModelToolDefinitionParameter> getParameters();

		/**
		 * Get the type of value the tool call is expected to return.
		 */
		ModelToolDefinitionParameter getReturnType();

	protected:

	private:

		/// Name of the tool.
		std::string _name;

		/// Description of the tool.
		std::string _description;

		/// Parameters the tool accepts.
		std::vector<ModelToolDefinitionParameter> _parameters;

		/// Type of value the tool is expected to return in string form.
		ModelToolDefinitionParameter _returnType;
};

#endif
