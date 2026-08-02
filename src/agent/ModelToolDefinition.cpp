#include "ModelToolDefinition.hpp"

namespace
{
	// Stand-in result for a tool that declared none. A tool result message must carry content on every
	// provider, so a tool with nothing to report is still answered, with whether its call completed.
	const char* const NO_RESULT_NAME = "ok";
	const char* const NO_RESULT_DESCRIPTION = "Whether the tool ran.";
}

ModelToolDefinition::ModelToolDefinition(std::string name, std::string description,
	std::vector<ModelToolDefinitionParameter> parameters, ModelToolDefinitionParameter returnType) :
	_name{name}, _description{description}, _parameters{parameters}, _returnType{returnType},
	_hasReturnType{true}
{
}

ModelToolDefinition::ModelToolDefinition(std::string name, std::string description,
	std::vector<ModelToolDefinitionParameter> parameters) :
	_name{name}, _description{description}, _parameters{parameters},
	_returnType{NO_RESULT_NAME, NO_RESULT_DESCRIPTION, ModelToolDefinitionParameter::PrimitiveType::BOOL},
	_hasReturnType{false}
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

bool ModelToolDefinition::hasReturnType()
{
	return _hasReturnType;
}
