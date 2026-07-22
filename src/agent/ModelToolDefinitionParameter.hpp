#ifndef MODEL_TOOL_DEFINITION_PARAMETER_H
#define MODEL_TOOL_DEFINITION_PARAMETER_H

#include <string>
#include <vector>

/**
 * Describes the type of a parameter for a tool that can be called by an AI model.
 */
class ModelToolDefinitionParameter
{
	public:

		/*
		 * Type of value this parameter accepts.
		 * @note For the moment ARRAY is omitted for simplicity.
		 */
		enum class Type
		{
			/// String.
			STRING,
			/// General number.
			NUMBER,
			/// Integer specific number.
			INTEGER,
			/// Boolean.
			BOOL
		};

		/**
		 * Create a description of a tool call parameter.
		 * @param name Name of the parameter.
		 * @param description Description of the parameter.
		 * @param type Type of value this parameter accepts.
		 * @param required If true, the model must supply this parameter when calling the tool.
		 * @param stringChoices If the type is STRING, restricts the parameter to this list of
		 *        allowed values. Ignored for other types. An empty list means the string is unrestricted.
		 */
		ModelToolDefinitionParameter(std::string name, std::string description, Type type, bool required = true,
			std::vector<std::string> stringChoices = {});

		/**
		 * Get the name of this parameter.
		 */
		std::string getName();

		/**
		 * Get the description of this parameter.
		 */
		std::string getDescription();

		/**
		 * Get the type of value this parameter accepts.
		 */
		Type getType();

		/**
		 * Get whether the model must supply this parameter when calling the tool.
		 */
		bool getRequired();

		/**
		 * Get the list of values the STRING type is restricted to.
		 * @note This would map to the enum field in an Ollama string parameter definition.
		 * @returns The allowed values, or an empty list if the string is unrestricted.
		 */
		std::vector<std::string> getStringChoices();

	protected:

	private:

		/// The name of the parameter.
		std::string _name;

		/// The description of the parameter.
		std::string _description;

		/// The type of value this parameter accepts.
		Type _type;

		/// True if the model must supply this parameter when calling the tool.
		bool _required;

		/// If the type is STRING, the list of values the parameter is restricted to.
		std::vector<std::string> _stringChoices;
};

#endif
