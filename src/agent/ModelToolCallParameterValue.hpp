#ifndef MODEL_TOOL_CALL_PARAMETER_VALUE_H
#define MODEL_TOOL_CALL_PARAMETER_VALUE_H

#include <string>
#include <variant>
#include <vector>

/**
 * A value supplied for a single parameter of a tool call made by an AI model.
 * @note The parameter this value is for is referenced by name only, so a value can be carried
 *       independently of the tool definition it belongs to. Use
 *       ModelToolDefinitionParameter::conformsTo() to check the value against the definition once
 *       that definition is to hand.
 */
class ModelToolCallParameterValue
{
	public:

		/**
		 * Every value a parameter of any ModelToolDefinitionParameter type can hold.
		 */
		typedef std::variant<std::string, double, long long, bool, std::vector<std::string>,
			std::vector<double>, std::vector<long long>, std::vector<bool>> Value;

		/**
		 * Create a value for a tool call parameter.
		 * @param parameterName Name of the parameter this value is for.
		 * @param value The value itself.
		 */
		ModelToolCallParameterValue(std::string parameterName, Value value);

		/**
		 * Get the name of the parameter this value is for.
		 */
		std::string getParameterName();

		/**
		 * Get the value.
		 */
		Value getValue();

	protected:

	private:

		/// Name of the parameter this value is for.
		std::string _parameterName;

		/// The value itself.
		Value _value;
};

#endif
