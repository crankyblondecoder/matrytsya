#include "AgentModel.hpp"

#include "AgentModelProvider.hpp"

AgentModel::AgentModel(Handle<AgentModelProvider> provider, std::string name, std::string description, std::string inputTokenCost, std::string outputTokenCost, bool free) :
	_provider{provider}, _name{name}, _description{description}, _inputTokenCost{inputTokenCost}, _outputTokenCost{outputTokenCost}, _free{free}
{
}

Handle<AgentModelProvider> AgentModel::getProvider()
{
	return _provider;
}

std::string AgentModel::getName()
{
	return _name;
}

std::string AgentModel::getDescription()
{
	return _description;
}

std::string AgentModel::getInputTokenCost()
{
	return _inputTokenCost;
}

std::string AgentModel::getOutputTokenCost()
{
	return _outputTokenCost;
}

bool AgentModel::getFree()
{
	return _free;
}
