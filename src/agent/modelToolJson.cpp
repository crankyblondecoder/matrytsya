#include "modelToolJson.hpp"

#include "AgentException.hpp"
#include "ModelToolBindings.hpp"
#include "ModelToolCallParameterValue.hpp"
#include "ModelToolDefinition.hpp"
#include "ModelToolDefinitionParameter.hpp"
#include "../log/log.hpp"
#include "../rapidjson/document.h"

#include <math.h>

namespace
{
	// -- Translation between the tool types and JSON, used only by the functions below --

	const char* __primitiveTypeName(ModelToolDefinitionParameter::PrimitiveType type)
	{
		switch(type)
		{
			case ModelToolDefinitionParameter::PrimitiveType::STRING:
				return "string";

			case ModelToolDefinitionParameter::PrimitiveType::NUMBER:
				return "number";

			case ModelToolDefinitionParameter::PrimitiveType::INTEGER:
				return "integer";

			case ModelToolDefinitionParameter::PrimitiveType::BOOL:
				return "boolean";
		}

		return "string";
	}

	void __writeParameterValue(ModelJsonWriter& writer, ModelToolCallParameterValue::Value value)
	{
		if(std::holds_alternative<std::string>(value))
		{
			writeJsonString(writer, std::get<std::string>(value));
		}
		else if(std::holds_alternative<double>(value))
		{
			writer.Double(std::get<double>(value));
		}
		else if(std::holds_alternative<long long>(value))
		{
			writer.Int64(std::get<long long>(value));
		}
		else if(std::holds_alternative<bool>(value))
		{
			writer.Bool(std::get<bool>(value));
		}
		else if(std::holds_alternative<std::vector<std::string>>(value))
		{
			writer.StartArray();

			for(const std::string& element : std::get<std::vector<std::string>>(value))
			{
				writeJsonString(writer, element);
			}

			writer.EndArray();
		}
		else if(std::holds_alternative<std::vector<double>>(value))
		{
			writer.StartArray();

			for(double element : std::get<std::vector<double>>(value)) writer.Double(element);

			writer.EndArray();
		}
		else if(std::holds_alternative<std::vector<long long>>(value))
		{
			writer.StartArray();

			for(long long element : std::get<std::vector<long long>>(value)) writer.Int64(element);

			writer.EndArray();
		}
		else
		{
			writer.StartArray();

			for(bool element : std::get<std::vector<bool>>(value)) writer.Bool(element);

			writer.EndArray();
		}
	}

	/**
	 * Build the content of a message carrying the result of a tool call.
	 * @param result Value the binding returned.
	 * @returns The message content.
	 */
	std::string __resultContent(ModelToolCallParameterValue result)
	{
		rapidjson::StringBuffer buffer;

		ModelJsonWriter writer(buffer);

		writer.StartObject();
			writeJsonKey(writer, result.getParameterName());
			__writeParameterValue(writer, result.getValue());
		writer.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	bool __primitiveValueFromJson(ModelToolDefinitionParameter::PrimitiveType type,
		const rapidjson::Value& jsonValue, ModelToolCallParameterValue::Value& value)
	{
		switch(type)
		{
			case ModelToolDefinitionParameter::PrimitiveType::STRING:
				if(!jsonValue.IsString()) return false;
				value = std::string(jsonValue.GetString(), jsonValue.GetStringLength());
				return true;

			case ModelToolDefinitionParameter::PrimitiveType::NUMBER:
				if(!jsonValue.IsNumber()) return false;
				value = jsonValue.GetDouble();
				return true;

			case ModelToolDefinitionParameter::PrimitiveType::INTEGER:
				if(!jsonValue.IsInt64()) return false;
				value = (long long) jsonValue.GetInt64();
				return true;

			case ModelToolDefinitionParameter::PrimitiveType::BOOL:
				if(!jsonValue.IsBool()) return false;
				value = jsonValue.GetBool();
				return true;
		}

		return false;
	}

	bool __arrayValueFromJson(ModelToolDefinitionParameter::ArrayType type,
		const rapidjson::Value& jsonValue, ModelToolCallParameterValue::Value& value)
	{
		if(!jsonValue.IsArray()) return false;

		switch(type.elementType)
		{
			case ModelToolDefinitionParameter::PrimitiveType::STRING:
			{
				std::vector<std::string> elements;

				for(const rapidjson::Value& elementValue : jsonValue.GetArray())
				{
					if(!elementValue.IsString()) return false;

					elements.push_back(std::string(elementValue.GetString(), elementValue.GetStringLength()));
				}

				value = elements;
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::NUMBER:
			{
				std::vector<double> elements;

				for(const rapidjson::Value& elementValue : jsonValue.GetArray())
				{
					if(!elementValue.IsNumber()) return false;

					elements.push_back(elementValue.GetDouble());
				}

				value = elements;
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::INTEGER:
			{
				std::vector<long long> elements;

				for(const rapidjson::Value& elementValue : jsonValue.GetArray())
				{
					if(!elementValue.IsInt64()) return false;

					elements.push_back((long long) elementValue.GetInt64());
				}

				value = elements;
				return true;
			}

			case ModelToolDefinitionParameter::PrimitiveType::BOOL:
			{
				std::vector<bool> elements;

				for(const rapidjson::Value& elementValue : jsonValue.GetArray())
				{
					if(!elementValue.IsBool()) return false;

					elements.push_back(elementValue.GetBool());
				}

				value = elements;
				return true;
			}
		}

		return false;
	}

	/**
	 * Convert a value the model supplied for a tool call parameter into the variant alternative the
	 * parameter's definition expects.
	 * @param parameter Definition of the parameter the value was supplied for.
	 * @param jsonValue Value the model supplied.
	 * @param value Set to the converted value.
	 * @returns False if the supplied value is not of the type the parameter declares.
	 */
	bool __parameterValueFromJson(ModelToolDefinitionParameter& parameter,
		const rapidjson::Value& jsonValue, ModelToolCallParameterValue::Value& value)
	{
		std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
			ModelToolDefinitionParameter::StringChoice> type = parameter.getType();

		if(std::holds_alternative<ModelToolDefinitionParameter::PrimitiveType>(type))
		{
			return __primitiveValueFromJson(std::get<ModelToolDefinitionParameter::PrimitiveType>(type),
				jsonValue, value);
		}

		if(std::holds_alternative<ModelToolDefinitionParameter::ArrayType>(type))
		{
			return __arrayValueFromJson(std::get<ModelToolDefinitionParameter::ArrayType>(type), jsonValue,
				value);
		}

		// Membership of the choices is left to conformsTo().
		if(!jsonValue.IsString()) return false;

		value = std::string(jsonValue.GetString(), jsonValue.GetStringLength());

		return true;
	}

	/**
	 * Build the parameter values for a tool call from the arguments the model supplied.
	 * @param definition Definition of the tool being called.
	 * @param argumentsValue Arguments the model supplied. May be null when it supplied none.
	 * @param parameterValues Filled with the values built.
	 * @param error Set to a description of the problem when the arguments cannot be used.
	 * @returns False if the arguments do not satisfy the definition, leaving error set.
	 */
	bool __buildParameterValues(ModelToolDefinition& definition, const rapidjson::Value* argumentsValue,
		std::vector<ModelToolCallParameterValue>& parameterValues, std::string& error)
	{
		std::vector<ModelToolDefinitionParameter> parameters = definition.getParameters();

		for(ModelToolDefinitionParameter& parameter : parameters)
		{
			std::string name = parameter.getName();

			const rapidjson::Value* memberValue = 0;

			if(argumentsValue && argumentsValue -> IsObject() && argumentsValue -> HasMember(name.c_str()))
			{
				memberValue = &(*argumentsValue)[name.c_str()];
			}

			if(!memberValue || memberValue -> IsNull())
			{
				if(!parameter.getRequired()) continue;

				error = "No value was supplied for the required parameter '" + name + "'.";

				return false;
			}

			ModelToolCallParameterValue::Value converted;

			if(!__parameterValueFromJson(parameter, *memberValue, converted))
			{
				error = "The value supplied for the parameter '" + name + "' is not of the expected type.";

				return false;
			}

			ModelToolCallParameterValue parameterValue(name, converted);

			if(!parameter.conformsTo(parameterValue))
			{
				error = "The value supplied for the parameter '" + name + "' is not a legal value.";

				return false;
			}

			parameterValues.push_back(parameterValue);
		}

		return true;
	}

	/**
	 * Service a single tool call, leaving what it is reported as to the caller.
	 * @param tools Tool bindings the request makes available.
	 * @param name Name of the tool the model called.
	 * @param argumentsValue Arguments the model supplied. May be null when it supplied none.
	 * @returns The content of the message to send back.
	 */
	std::string __processToolCall(std::vector<Handle<ModelToolBindings>>& tools, const std::string& name,
		const rapidjson::Value* argumentsValue)
	{
		for(Handle<ModelToolBindings>& toolHandle : tools)
		{
			ModelToolBindings* bindings = toolHandle.getInstance();

			if(!bindings || !bindings -> hasBinding(name)) continue;

			std::vector<ModelToolDefinition> definitions = bindings -> getModelToolDefinitions();

			for(ModelToolDefinition& definition : definitions)
			{
				if(definition.getName() != name) continue;

				std::vector<ModelToolCallParameterValue> parameterValues;

				std::string error;

				if(!__buildParameterValues(definition, argumentsValue, parameterValues, error))
				{
					return jsonToolErrorContent(error);
				}

				// A binding that fails is reported to the model rather than abandoning the request, so that
				// it can correct itself.
				try
				{
					return __resultContent(bindings -> processBinding(name, parameterValues));
				}
				catch(AgentException& exception)
				{
					return jsonToolErrorContent(exception.getDescription());
				}
				catch(Exception& exception)
				{
					return jsonToolErrorContent("The tool call failed.");
				}
			}

			// The binding claims the name but publishes no definition for it, so there is no way to know
			// what the arguments should have been.
			return jsonToolErrorContent("The tool '" + name + "' cannot be called.");
		}

		return jsonToolErrorContent("There is no tool named '" + name + "'.");
	}
}

void writeJsonString(ModelJsonWriter& writer, const std::string& value)
{
	writer.String(value.c_str(), (rapidjson::SizeType) value.size());
}

void writeJsonKey(ModelJsonWriter& writer, const std::string& key)
{
	writer.Key(key.c_str(), (rapidjson::SizeType) key.size());
}

void writeJsonTemperature(ModelJsonWriter& writer, double temperature)
{
	double scale = pow(10.0, MODEL_TEMPERATURE_DECIMAL_PLACES);

	// Rounded rather than truncated, so that a value sitting just under a place, as a double often does,
	// is sent as the place the caller wrote rather than as the one below it.
	writer.Double(round(temperature * scale) / scale);
}

std::string serialiseJsonValue(const rapidjson::Value& value)
{
	rapidjson::StringBuffer buffer;

	ModelJsonWriter writer(buffer);

	value.Accept(writer);

	return std::string(buffer.GetString(), buffer.GetSize());
}

void writeJsonToolParameterSchema(ModelJsonWriter& writer, ModelToolDefinitionParameter& parameter)
{
	std::variant<ModelToolDefinitionParameter::PrimitiveType, ModelToolDefinitionParameter::ArrayType,
		ModelToolDefinitionParameter::StringChoice> type = parameter.getType();

	writer.StartObject();

	if(std::holds_alternative<ModelToolDefinitionParameter::PrimitiveType>(type))
	{
		writer.Key("type");
		writer.String(__primitiveTypeName(std::get<ModelToolDefinitionParameter::PrimitiveType>(type)));
	}
	else if(std::holds_alternative<ModelToolDefinitionParameter::ArrayType>(type))
	{
		writer.Key("type");
		writer.String("array");

		writer.Key("items");
		writer.StartObject();
			writer.Key("type");
			writer.String(__primitiveTypeName(
				std::get<ModelToolDefinitionParameter::ArrayType>(type).elementType));
		writer.EndObject();
	}
	else
	{
		writer.Key("type");
		writer.String("string");

		writer.Key("enum");
		writer.StartArray();

		for(const std::string& choice :
			std::get<ModelToolDefinitionParameter::StringChoice>(type).stringChoices)
		{
			writeJsonString(writer, choice);
		}

		writer.EndArray();
	}

	writer.Key("description");
	writeJsonString(writer, parameter.getDescription());

	writer.EndObject();
}

void writeJsonToolParametersSchema(ModelJsonWriter& writer, ModelToolDefinition& definition)
{
	std::vector<ModelToolDefinitionParameter> parameters = definition.getParameters();

	writer.StartObject();
		writer.Key("type");
		writer.String("object");

		writer.Key("properties");
		writer.StartObject();

		for(ModelToolDefinitionParameter& parameter : parameters)
		{
			writeJsonKey(writer, parameter.getName());

			writeJsonToolParameterSchema(writer, parameter);
		}

		writer.EndObject();

		writer.Key("required");
		writer.StartArray();

		for(ModelToolDefinitionParameter& parameter : parameters)
		{
			if(parameter.getRequired()) writeJsonString(writer, parameter.getName());
		}

		writer.EndArray();
	writer.EndObject();
}

std::string jsonToolErrorContent(const std::string& message)
{
	rapidjson::StringBuffer buffer;

	ModelJsonWriter writer(buffer);

	writer.StartObject();
		writer.Key("error");
		writeJsonString(writer, message);
	writer.EndObject();

	return std::string(buffer.GetString(), buffer.GetSize());
}

std::string processJsonToolCall(std::vector<Handle<ModelToolBindings>>& tools, const std::string& name,
	const rapidjson::Value* argumentsValue)
{
	std::string content = __processToolCall(tools, name, argumentsValue);

	// Reported here rather than by each provider, as this is the one place every tool call a model asks for
	// is serviced. Nothing else shows what a binding handed back: a call that fails is only ever described
	// to the model, and one that succeeds is folded into a conversation that no one is watching.
	LOG(Logger::LogLevel::DEBUG, "Tool call '" + name + "' with arguments "
		+ (argumentsValue ? serialiseJsonValue(*argumentsValue) : std::string("{}")) + " returned "
		+ content + ".")

	return content;
}
