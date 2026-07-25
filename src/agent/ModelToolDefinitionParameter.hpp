#ifndef MODEL_TOOL_DEFINITION_PARAMETER_H
#define MODEL_TOOL_DEFINITION_PARAMETER_H

#include <string>
#include <variant>
#include <vector>

class ModelToolCallParameterValue;

/**
 * Describes the type of a parameter for a tool that can be called by an AI model.
 */
class ModelToolDefinitionParameter
{
	public:

		/**
		 * Primitive parameter value types.
		 */
		enum class PrimitiveType
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
		 * Array type with elements of same fixed type.
		 */
		struct ArrayType
		{
			PrimitiveType elementType;
		};

		/**
		 * String that is restricted to a list of choices.
		 */
		struct StringChoice
		{
			std::vector<std::string> stringChoices;
		};

		/**
		 * Create a description of a tool call parameter.
		 * @param name Name of the parameter.
		 * @param description Description of the parameter.
		 * @param type Type of value this parameter accepts.
		 * @param required If true, the model must supply this parameter when calling the tool.
		 */
		ModelToolDefinitionParameter(std::string name, std::string description, std::variant<PrimitiveType,
			ArrayType, StringChoice> type, bool required = true);

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
		std::variant<PrimitiveType, ArrayType, StringChoice> getType();

		/**
		 * Get whether the model must supply this parameter when calling the tool.
		 */
		bool getRequired();

		/**
		 * Check that a tool call value is a legal value for this parameter.
		 * @param value Value to check.
		 * @returns True if the value is for the parameter of this name and satisfies the type this
		 *          parameter accepts.
		 */
		bool conformsTo(ModelToolCallParameterValue& value);


	protected:

	private:

		bool __conformsToPrimitive(PrimitiveType type, ModelToolCallParameterValue& value);

		bool __conformsToArray(ArrayType type, ModelToolCallParameterValue& value);

		bool __conformsToStringChoice(StringChoice type, ModelToolCallParameterValue& value);

		/// The name of the parameter.
		std::string _name;

		/// The description of the parameter.
		std::string _description;

		/// The type of value this parameter accepts.
		std::variant<PrimitiveType, ArrayType, StringChoice> _type;

		/// True if the model must supply this parameter when calling the tool.
		bool _required;
};

#endif
