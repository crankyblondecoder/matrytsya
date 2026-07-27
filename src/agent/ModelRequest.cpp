#include "ModelRequest.hpp"

#include "AgentException.hpp"
#include "ModelContext.hpp"

ModelRequest::ModelRequest(Handle<ModelContext> context, ModelPrompt prompt, double temperature) :
	_context{context}, _prompt{prompt}, _temperature{temperature}
{
	if(!_context.isValid() || _prompt.getPrompt().empty())
	{
		throw AgentException(AgentException::EMPTY_MODEL_REQUEST);
	}

	// Checked here as well as wherever the temperature was taken from a caller, so that no provider has
	// to, whatever route the request was built by.
	checkTemperature(_temperature);
}

void ModelRequest::checkTemperature(double temperature)
{
	if(temperature == PROVIDER_DEFAULT_TEMPERATURE) return;

	if(temperature < 0.0 || temperature > MAX_TEMPERATURE)
	{
		throw AgentException(AgentException::INVALID_TEMPERATURE);
	}
}

Handle<ModelContext> ModelRequest::getContext()
{
	return _context;
}

ModelPrompt ModelRequest::getPrompt()
{
	return _prompt;
}

double ModelRequest::getTemperature()
{
	return _temperature;
}
