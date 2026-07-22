#include "ModelToolDefinition.hpp"

ModelToolDefinition::ModelToolDefinition(std::string name, std::string description,
	std::vector<ModelToolDefinitionParameter> parameters, ModelToolDefinitionParameter returnType) :
	_name{name}, _description{description}, _parameters{parameters}, _returnType{returnType}
{
}

std::string ModelToolDefinition::getName()
{
	return _name;
}

std::string ModelToolDefinition::getDescription()
{
	return _description;
}

std::vector<ModelToolDefinitionParameter> ModelToolDefinition::getParameters()
{
	return _parameters;
}

ModelToolDefinitionParameter ModelToolDefinition::getReturnType()
{
	return _returnType;
}
