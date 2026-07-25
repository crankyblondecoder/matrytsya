#include "ModelToolDefinitionParameter.hpp"

#include "ModelToolCallParameterValue.hpp"

#include <algorithm>

ModelToolDefinitionParameter::ModelToolDefinitionParameter(std::string name, std::string description,
	std::variant<PrimitiveType, ArrayType, StringChoice> type, bool required) :
	_name{name}, _description{description}, _type{type}, _required{required}
{
}

std::string ModelToolDefinitionParameter::getName()
{
	return _name;
}

std::string ModelToolDefinitionParameter::getDescription()
{
	return _description;
}

std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
	ModelToolDefinitionParameter::StringChoice> ModelToolDefinitionParameter::getType()
{
	return _type;
}

bool ModelToolDefinitionParameter::getRequired()
{
	return _required;
}

bool ModelToolDefinitionParameter::conformsTo(ModelToolCallParameterValue& value)
{
	if (value.getParameterName() != _name)
	{
		return false;
	}

	if (std::holds_alternative<PrimitiveType>(_type))
	{
		return __conformsToPrimitive(std::get<PrimitiveType>(_type), value);
	}

	if (std::holds_alternative<ArrayType>(_type))
	{
		return __conformsToArray(std::get<ArrayType>(_type), value);
	}

	return __conformsToStringChoice(std::get<StringChoice>(_type), value);
}

bool ModelToolDefinitionParameter::__conformsToPrimitive(PrimitiveType type,
	ModelToolCallParameterValue& value)
{
	ModelToolCallParameterValue::Value heldValue = value.getValue();

	switch (type)
	{
		case PrimitiveType::STRING:
			return std::holds_alternative<std::string>(heldValue);

		case PrimitiveType::NUMBER:
			// An integer is an acceptable general number, but not the other way around.
			return std::holds_alternative<double>(heldValue) || std::holds_alternative<long long>(heldValue);

		case PrimitiveType::INTEGER:
			return std::holds_alternative<long long>(heldValue);

		case PrimitiveType::BOOL:
			return std::holds_alternative<bool>(heldValue);
	}

	return false;
}

bool ModelToolDefinitionParameter::__conformsToArray(ArrayType type, ModelToolCallParameterValue& value)
{
	ModelToolCallParameterValue::Value heldValue = value.getValue();

	switch (type.elementType)
	{
		case PrimitiveType::STRING:
			return std::holds_alternative<std::vector<std::string>>(heldValue);

		case PrimitiveType::NUMBER:
			return std::holds_alternative<std::vector<double>>(heldValue)
				|| std::holds_alternative<std::vector<long long>>(heldValue);

		case PrimitiveType::INTEGER:
			return std::holds_alternative<std::vector<long long>>(heldValue);

		case PrimitiveType::BOOL:
			return std::holds_alternative<std::vector<bool>>(heldValue);
	}

	return false;
}

bool ModelToolDefinitionParameter::__conformsToStringChoice(StringChoice type,
	ModelToolCallParameterValue& value)
{
	ModelToolCallParameterValue::Value heldValue = value.getValue();

	if (!std::holds_alternative<std::string>(heldValue))
	{
		return false;
	}

	const std::string& heldString = std::get<std::string>(heldValue);

	return std::find(type.stringChoices.begin(), type.stringChoices.end(), heldString)
		!= type.stringChoices.end();
}
