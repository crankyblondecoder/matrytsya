#ifndef MODEL_TOOL_JSON_H
#define MODEL_TOOL_JSON_H

// JSON translation of model tools.
//
// Every provider that accepts tools describes them as JSON Schema and supplies the arguments of a
// tool call as JSON, so none of the translation between those and ModelToolDefinition /
// ModelToolCallParameterValue is tied to a particular one. What is left to a provider is only its
// own wire format: how it wraps a tool definition, and how it lays out a request and its response.

#include <string>
#include <vector>

#include "../rapidjson/fwd.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"
#include "../util/Handle.hpp"

class ModelToolBindings;
class ModelToolDefinition;
class ModelToolDefinitionParameter;

/// Writer used to build the JSON of a request to a model.
typedef rapidjson::Writer<rapidjson::StringBuffer> ModelJsonWriter;

// Decimal places a sampling temperature is cut to before it is sent to a provider.
#define MODEL_TEMPERATURE_DECIMAL_PLACES 3

/**
 * Write a string, which need not be null terminated.
 * @param writer Writer to write to.
 * @param value String to write.
 */
void writeJsonString(ModelJsonWriter& writer, const std::string& value);

/**
 * Write an object key, which need not be null terminated.
 * @param writer Writer to write to.
 * @param key Key to write.
 */
void writeJsonKey(ModelJsonWriter& writer, const std::string& key);

/**
 * Write a sampling temperature, cut to MODEL_TEMPERATURE_DECIMAL_PLACES decimal places.
 * @param writer Writer to write to.
 * @param temperature Temperature to write.
 * @note Only the value is written, since where it sits in a request and what it is called there differ
 *       between providers. What is common to all of them is the cut: the binary fraction a double holds
 *       for a value like 0.2 renders as a long run of digits that claims a precision the caller never
 *       asked for, and no provider has a use for it.
 */
void writeJsonTemperature(ModelJsonWriter& writer, double temperature);

/**
 * Render a parsed JSON value back to its text form.
 * @param value Value to render.
 * @returns The JSON text of the value.
 * @note Useful for holding on to part of a response once the document that owns it has gone, e.g.
 *       to echo the tool calls a model asked for back to it on the next request.
 */
std::string serialiseJsonValue(const rapidjson::Value& value);

/**
 * Write the JSON Schema of a single tool parameter.
 * @param writer Writer to write to.
 * @param parameter Parameter to describe.
 * @note A parameter restricted to a list of choices is written as a string with an "enum".
 */
void writeJsonToolParameterSchema(ModelJsonWriter& writer, ModelToolDefinitionParameter& parameter);

/**
 * Write the JSON Schema object describing everything a tool accepts, i.e. its "properties" and which
 * of them are "required".
 * @param writer Writer to write to.
 * @param definition Tool whose parameters are to be described.
 * @note This is the value a provider gives its own name to, e.g. "parameters" or "input_schema".
 */
void writeJsonToolParametersSchema(ModelJsonWriter& writer, ModelToolDefinition& definition);

/**
 * Build the content of a message reporting that a tool call could not be serviced.
 * @param message Description of what went wrong, for the model to act on.
 * @returns The message content.
 */
std::string jsonToolErrorContent(const std::string& message);

/**
 * Service a single tool call a model asked for.
 * @param tools Tool bindings the request makes available.
 * @param name Name of the tool the model called.
 * @param argumentsValue Arguments the model supplied. May be null when it supplied none.
 * @returns The content of the message to send back, carrying either the result of the call or an
 *          error the model is given the chance to recover from.
 * @note A failed call is reported to the model rather than thrown, so that it can correct itself.
 *       Whatever bounds how long it may keep trying is left to the caller.
 */
std::string processJsonToolCall(std::vector<Handle<ModelToolBindings>>& tools, const std::string& name,
	const rapidjson::Value* argumentsValue);

#endif
