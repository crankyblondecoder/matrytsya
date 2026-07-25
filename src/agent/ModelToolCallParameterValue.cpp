#include "ModelToolCallParameterValue.hpp"

ModelToolCallParameterValue::ModelToolCallParameterValue(std::string parameterName, Value value) :
	_parameterName{parameterName}, _value{value}
{
}

std::string ModelToolCallParameterValue::getParameterName()
{
	return _parameterName;
}

ModelToolCallParameterValue::Value ModelToolCallParameterValue::getValue()
{
	return _value;
}
