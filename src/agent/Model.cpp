#include "Model.hpp"

#include "ModelProvider.hpp"
#include "ModelRequest.hpp"

#include <iostream>

bool Model::_logToConsole = false;

Model::Model(Handle<ModelProvider> provider, std::string name, std::string description,
	std::string inputTokenCost, std::string outputTokenCost, bool free) :
	_provider{provider}, _name{name}, _description{description}, _inputTokenCost{inputTokenCost}, _outputTokenCost{outputTokenCost}, _free{free}
{
}

Model::~Model()
{
}

Handle<ModelProvider> Model::getProvider()
{
	return _provider;
}

std::string Model::getName()
{
	return _name;
}

std::string Model::getDescription()
{
	return _description;
}

std::string Model::getInputTokenCost()
{
	return _inputTokenCost;
}

std::string Model::getOutputTokenCost()
{
	return _outputTokenCost;
}

bool Model::getFree()
{
	return _free;
}

std::string Model::_processRequest(ModelRequest& request)
{
	if(_logToConsole)
	{
		std::cout << "----- Model request (" << _name << ") -----" << std::endl
			<< request.getPrompt().getPrompt() << std::endl;
	}

	std::string response = getProvider().getInstance() -> processRequest(Handle<Model>(this), request);

	if(_logToConsole)
	{
		std::cout << "----- Model response (" << _name << ") -----" << std::endl
			<< response << std::endl;
	}

	return response;
}
