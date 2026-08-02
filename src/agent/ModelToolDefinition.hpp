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
		 * Create a description of a tool call that has no result of its own, i.e. one that does something
		 * rather than reports something.
		 * @param name Name of the tool.
		 * @param description Description of the tool.
		 * @param parameters Parameters the tool accepts.
		 * @note No provider's tool schema has anywhere to declare a return type, so what a tool hands back is
		 *       only ever described to a model in prose. A tool with nothing to report has no such prose to
		 *       offer, and hasReturnType() is what says so.
		 * @note A tool call is still answered: no provider permits a tool result message with no content. The
		 *       return type this leaves behind is a boolean named "ok", carrying whether the call completed,
		 *       so that whatever services the call has a name to answer under without inventing one.
		 */
		ModelToolDefinition(std::string name, std::string description,
				std::vector<ModelToolDefinitionParameter> parameters);

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
		 * @note Always a type, even for a tool that declared none; hasReturnType() is what separates a type
		 *       the tool declared from the stand-in one given to a tool with no result.
		 */
		ModelToolDefinitionParameter getReturnType();

		/**
		 * Get whether the tool declared a return type of its own.
		 * @returns True if getReturnType() describes something the tool reports. False if the tool has no
		 *          result and the type it carries is only there so its call has something to answer with.
		 */
		bool hasReturnType();

	protected:

	private:

		/// Name of the tool.
		std::string _name;

		/// Description of the tool.
		std::string _description;

		/// Parameters the tool accepts.
		std::vector<ModelToolDefinitionParameter> _parameters;

		/// Type of value the tool is expected to return in string form. Always populated; for a tool with no
		/// result of its own this is the stand-in the no return type constructor leaves behind.
		ModelToolDefinitionParameter _returnType;

		/// Whether _returnType is one the tool declared rather than the stand-in.
		bool _hasReturnType;
};

#endif
