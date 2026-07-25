#include "ModelToolBindings.hpp"

#include "AgentException.hpp"
#include "ModelToolCallParameterValue.hpp"

#include <algorithm>

bool ModelToolBindings::hasBinding(std::string name)
{
	return std::find(_bindingNames.begin(), _bindingNames.end(), name) != _bindingNames.end();
}

void ModelToolBindings::_registerBinding(std::string name)
{
	_bindingNames.push_back(name);
}

ModelToolCallParameterValue& ModelToolBindings::_getParameterValue(
	std::vector<ModelToolCallParameterValue>& parameterValues, std::string parameterName)
{
	for(ModelToolCallParameterValue& parameterValue : parameterValues)
	{
		if(parameterValue.getParameterName() == parameterName)
		{
			return parameterValue;
		}
	}

	throw AgentException(AgentException::PARAMETER_NOT_FOUND);
}
