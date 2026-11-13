#include "Model.hpp"

#include "AgentException.hpp"
#include "ModelProvider.hpp"
#include "ModelRequest.hpp"
#include "../log/log.hpp"

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

	std::string response;

	try
	{
		response = getProvider().getInstance() -> processRequest(Handle<Model>(this), request);
	}
	catch(AgentException& exception)
	{
		// Logged here as well as reported to the caller, as an agent node's request has nowhere else to say why
		// it failed, and a provider's own explanation is what tells a rate limit apart from a broken request.
		LOG(Logger::LogLevel::ERROR, "Model request (" + _name + ") failed: " + exception.getDescription())

		throw;
	}

	if(_logToConsole)
	{
		std::cout << "----- Model response (" << _name << ") -----" << std::endl
			<< response << std::endl;
	}

	return response;
}
