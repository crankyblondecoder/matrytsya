#include "ModelToolDefinitionParameter.hpp"

ModelToolDefinitionParameter::ModelToolDefinitionParameter(std::string name, std::string description, Type type,
	bool required, std::vector<std::string> stringChoices) :
	_name{name}, _description{description}, _type{type}, _required{required},
	_stringChoices{stringChoices}
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

ModelToolDefinitionParameter::Type ModelToolDefinitionParameter::getType()
{
	return _type;
}

bool ModelToolDefinitionParameter::getRequired()
{
	return _required;
}

std::vector<std::string> ModelToolDefinitionParameter::getStringChoices()
{
	return _stringChoices;
}
