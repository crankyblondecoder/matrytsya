#include "OpenRouterModelProvider.hpp"

#include "AgentException.hpp"
#include "Model.hpp"
#include "ModelContext.hpp"
#include "ModelPrompt.hpp"
#include "ModelRequest.hpp"
#include "ModelSystemPrompt.hpp"
#include "ModelToolBindings.hpp"
#include "ModelToolDefinition.hpp"
#include "OpenRouterModel.hpp"
#include "modelToolJson.hpp"
#include "../thread/ThreadBase.hpp"
#include "../rapidjson/document.h"

#include <stdlib.h>

namespace
{
	// -- OpenRouter's (OpenAI style) chat wire format, used only by _processRequest --

	/// One entry of the chat history accumulated across rounds of tool calls.
	struct ChatMessage
	{
		/// Role the message is attributed to, i.e. "system", "user", "assistant" or "tool".
		std::string role;
		std::string content;

		/// Serialised tool_calls array echoed back to the model. Empty when the message had none.
		std::string toolCalls;

		/// Identifier of the call this message carries the result of. Empty when not a tool result.
		std::string toolCallId;
	};

	/**
	 * Read a string member of a JSON object.
	 * @param value Object to read from.
	 * @param name Name of the member.
	 * @returns The member's value. Empty when it is missing or not a string.
	 */
	std::string __stringMember(const rapidjson::Value& value, const char* name)
	{
		if(!value.IsObject() || !value.HasMember(name) || !value[name].IsString()) return "";

		return std::string(value[name].GetString(), value[name].GetStringLength());
	}

	/**
	 * Build the tool_calls array of an assistant message from tool calls recorded in a context.
	 * @param toolCalls Calls the model asked for in one round.
	 * @returns The JSON text of the array.
	 * @note Only needed for replaying a chat history. The calls of the round in hand are echoed back
	 *       exactly as the model asked for them, which costs nothing and cannot drift from its wording.
	 */
	std::string __buildToolCallsJson(std::vector<ModelContext::ToolCall>& toolCalls)
	{
		rapidjson::StringBuffer buffer;

		ModelJsonWriter writer(buffer);

		writer.StartArray();

		for(ModelContext::ToolCall& toolCall : toolCalls)
		{
			writer.StartObject();
				writer.Key("id");
				writeJsonString(writer, toolCall.id);

				writer.Key("type");
				writer.String("function");

				writer.Key("function");
				writer.StartObject();
					writer.Key("name");
					writeJsonString(writer, toolCall.name);

					// The API carries arguments as the text of a JSON object rather than as the object.
					writer.Key("arguments");
					writeJsonString(writer, toolCall.arguments.empty() ? "{}" : toolCall.arguments);
				writer.EndObject();
			writer.EndObject();
		}

		writer.EndArray();

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	std::string __buildRequestBody(const std::string& modelName, std::vector<ChatMessage>& messages,
		std::vector<Handle<ModelToolBindings>>& tools, double temperature)
	{
		rapidjson::StringBuffer buffer;

		ModelJsonWriter writer(buffer);

		writer.StartObject();
			writer.Key("model");
			writeJsonString(writer, modelName);

			writer.Key("messages");
			writer.StartArray();

			for(ChatMessage& message : messages)
			{
				writer.StartObject();
					writer.Key("role");
					writeJsonString(writer, message.role);

					writer.Key("content");
					writeJsonString(writer, message.content);

					if(!message.toolCallId.empty())
					{
						writer.Key("tool_call_id");
						writeJsonString(writer, message.toolCallId);
					}

					if(!message.toolCalls.empty())
					{
						writer.Key("tool_calls");
						writer.RawValue(message.toolCalls.c_str(), message.toolCalls.size(),
							rapidjson::kArrayType);
					}
				writer.EndObject();
			}

			writer.EndArray();

			// A context need not offer any tools, and a model asked to choose from an empty list is being
			// told something that was not meant.
			if(!tools.empty())
			{
				writer.Key("tools");
				writer.StartArray();

				for(Handle<ModelToolBindings>& toolHandle : tools)
				{
					ModelToolBindings* bindings = toolHandle.getInstance();

					if(!bindings) continue;

					std::vector<ModelToolDefinition> definitions = bindings -> getModelToolDefinitions();

					for(ModelToolDefinition& definition : definitions)
					{
						writeJsonToolDefinition(writer, definition);
					}
				}

				writer.EndArray();
			}

			// Left off entirely where no temperature was asked for, so that the model's own default stands
			// rather than being overwritten with a guess at what it is.
			if(temperature != ModelRequest::PROVIDER_DEFAULT_TEMPERATURE)
			{
				writer.Key("temperature");
				writeJsonTemperature(writer, temperature);
			}
		writer.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize());
	}
}

void OpenRouterModelProvider::__throwProviderError(const rapidjson::Value& errorValue, AgentException::Error fallback)
{
	if(!errorValue.IsObject())
	{
		throw AgentException(fallback, errorValue.IsString() ?
			_truncateDetail(std::string(errorValue.GetString(), errorValue.GetStringLength())) : "");
	}

	int code = 0;

	if(errorValue.HasMember("code"))
	{
		const rapidjson::Value& codeValue = errorValue["code"];

		if(codeValue.IsInt()) code = codeValue.GetInt();
		else if(codeValue.IsString()) code = atoi(codeValue.GetString());
	}

	std::string detail = __stringMember(errorValue, "message");

	if(errorValue.HasMember("metadata") && errorValue["metadata"].IsObject())
	{
		const rapidjson::Value& metadataValue = errorValue["metadata"];

		std::string raw;

		if(metadataValue.HasMember("raw"))
		{
			raw = metadataValue["raw"].IsString() ? __stringMember(metadataValue, "raw") :
				serialiseJsonValue(metadataValue["raw"]);
		}

		if(!raw.empty()) detail += (detail.empty() ? "" : ": ") + _truncateDetail(raw);

		std::string providerName = __stringMember(metadataValue, "provider_name");

		if(!providerName.empty()) detail += " (" + providerName + ")";
	}

	if(code != 0) detail = "HTTP " + std::to_string(code) + (detail.empty() ? "" : ": " + detail);

	switch(code)
	{
		case 401:
			throw AgentException(AgentException::AUTHENTICATION_FAILED, detail);

		case 402:
			throw AgentException(AgentException::INSUFFICIENT_CREDITS, detail);

		case 429:
			throw AgentException(AgentException::MODEL_RATE_LIMITED, detail);
	}

	throw AgentException(fallback, detail);
}

OpenRouterModelProvider::~OpenRouterModelProvider()
{
}

OpenRouterModelProvider::OpenRouterModelProvider(std::string url, std::string apiKey) :
	_url{url}, _apiKey{apiKey}
{
	_checkConnection(_url);

	// Checked up front, as the model list is served without a key and would not catch a bad one; left
	// alone it would only come to light on the first request a model is sent.
	__checkApiKey();

	_populateModels();
}

std::string OpenRouterModelProvider::_requestHeaders()
{
	return "Authorization: Bearer " + _apiKey + "\r\nX-Title: Matrytsya\r\n";
}

void OpenRouterModelProvider::__checkApiKey()
{
	std::string body = _httpGet(_url + "/key");

	rapidjson::Document document;

	document.Parse(body.c_str());

	// A key that is accepted is answered with a description of it; anything else is a rejection.
	if(document.HasParseError() || !document.IsObject())
	{
		throw AgentException(AgentException::AUTHENTICATION_FAILED, _unreadableResponseDetail(body));
	}

	if(document.HasMember("error"))
	{
		__throwProviderError(document["error"], AgentException::AUTHENTICATION_FAILED);
	}

	if(!document.HasMember("data") || !document["data"].IsObject())
	{
		throw AgentException(AgentException::AUTHENTICATION_FAILED, _unreadableResponseDetail(body));
	}
}

void OpenRouterModelProvider::_populateModels()
{
	std::string body = _httpGet(_url + "/models");

	rapidjson::Document document;

	document.Parse(body.c_str());

	if(document.HasParseError() || !document.IsObject())
	{
		throw AgentException(AgentException::MODEL_FETCH_FAILED, _unreadableResponseDetail(body));
	}

	if(document.HasMember("error"))
	{
		__throwProviderError(document["error"], AgentException::MODEL_PROVIDER_ERROR);
	}

	if(!document.HasMember("data") || !document["data"].IsArray())
	{
		throw AgentException(AgentException::MODEL_FETCH_FAILED, _unreadableResponseDetail(body));
	}

	for(const rapidjson::Value& modelValue : document["data"].GetArray())
	{
		std::string id = __stringMember(modelValue, "id");

		if(id.empty())
		{
			throw AgentException(AgentException::MODEL_FETCH_FAILED, "A model in the list had no id.");
		}

		std::string inputTokenCost;
		std::string outputTokenCost;

		if(modelValue.HasMember("pricing"))
		{
			inputTokenCost = __stringMember(modelValue["pricing"], "prompt");
			outputTokenCost = __stringMember(modelValue["pricing"], "completion");
		}

		// Only a model that is priced at nothing both ways is free; one whose price is not given is not
		// assumed to be.
		bool free = inputTokenCost == "0" && outputTokenCost == "0";

		OpenRouterModel* model = new OpenRouterModel(this, id, __stringMember(modelValue, "name"),
			inputTokenCost, outputTokenCost, free);

		_addModel(Handle<Model>(model));

		// _addModel takes its own ref via the Handle; release the implicit construction ref.
		model -> decrRef();
	}
}

std::string OpenRouterModelProvider::_processRequest(Handle<Model> model, ModelRequest& request,
	std::vector<ModelContext::ToolCallRound>& toolCallRounds)
{
	Handle<ModelContext> contextHandle = request.getContext();

	// Held by the handle above for the whole of this call, so the raw pointer cannot outlive the context.
	ModelContext* context = contextHandle.getInstance();

	std::vector<ChatMessage> messages;

	std::vector<ModelSystemPrompt> systemPrompts = context -> getSystemPrompts();

	for(ModelSystemPrompt& systemPrompt : systemPrompts)
	{
		messages.push_back(ChatMessage{"system", systemPrompt.getPrompt(), "", ""});
	}

	std::vector<ModelContext::ChatExchange> chatHistory = context -> getChatHistory();

	for(ModelContext::ChatExchange& exchange : chatHistory)
	{
		messages.push_back(ChatMessage{"user", exchange.prompt.getPrompt(), "", ""});

		// What the model asked for and what it was told has to be replayed in the order it happened, or a
		// later round of the same exchange reads as though it were answered out of nothing.
		for(ModelContext::ToolCallRound& toolCallRound : exchange.toolCallRounds)
		{
			messages.push_back(ChatMessage{"assistant", toolCallRound.content,
				__buildToolCallsJson(toolCallRound.toolCalls), ""});

			for(ModelContext::ToolCall& toolCall : toolCallRound.toolCalls)
			{
				messages.push_back(ChatMessage{"tool", toolCall.result, "", toolCall.id});
			}
		}

		messages.push_back(ChatMessage{"assistant", exchange.response, "", ""});
	}

	// The prompt yet to be processed closes the conversation, so the model answers that one.
	messages.push_back(ChatMessage{"user", request.getPrompt().getPrompt(), "", ""});

	std::vector<Handle<ModelToolBindings>> tools = context -> getTools();

	std::string modelName = model.getInstance() -> getName();

	// Read once, so that every round of the same request is sampled the same way.
	double temperature = request.getTemperature();

	for(unsigned round = 0; round < MAX_TOOL_CALL_ROUNDS; ++round)
	{
		// The POST abandons its own wait, but the tool calls a round asks for are run between two of them
		// and can take a while themselves, so a stop asked for during one is caught here rather than
		// letting it start a further request that is only going to be thrown away.
		if(ThreadBase::currentThreadStopping())
		{
			throw AgentException(AgentException::REQUEST_ABORTED);
		}

		std::string body = _httpPost(_url + "/chat/completions",
			__buildRequestBody(modelName, messages, tools, temperature));

		rapidjson::Document document;

		document.Parse(body.c_str());

		if(document.HasParseError() || !document.IsObject())
		{
			throw AgentException(AgentException::MODEL_REQUEST_FAILED, _unreadableResponseDetail(body));
		}

		// OpenRouter reports its own failures, and those of the host serving the model before it starts
		// answering, e.g. an unknown model, exhausted credit or a rate limit, as a top level "error".
		if(document.HasMember("error"))
		{
			__throwProviderError(document["error"], AgentException::MODEL_PROVIDER_ERROR);
		}

		if(!document.HasMember("choices") || !document["choices"].IsArray() || document["choices"].Empty()
			|| !document["choices"][0].IsObject())
		{
			throw AgentException(AgentException::MODEL_REQUEST_FAILED, _unreadableResponseDetail(body));
		}

		// A host that fails part way through answering reports it on the choice itself instead.
		if(document["choices"][0].HasMember("error"))
		{
			__throwProviderError(document["choices"][0]["error"], AgentException::MODEL_PROVIDER_ERROR);
		}

		if(!document["choices"][0].HasMember("message") || !document["choices"][0]["message"].IsObject())
		{
			throw AgentException(AgentException::MODEL_REQUEST_FAILED, _unreadableResponseDetail(body));
		}

		const rapidjson::Value& messageValue = document["choices"][0]["message"];

		// Sent as null rather than as an empty string when the model only asks for tool calls.
		std::string content = __stringMember(messageValue, "content");

		if(!messageValue.HasMember("tool_calls") || !messageValue["tool_calls"].IsArray()
			|| messageValue["tool_calls"].Empty())
		{
			return content;
		}

		const rapidjson::Value& toolCallsValue = messageValue["tool_calls"];

		// The model has to see the calls it asked for alongside the results it gets back.
		messages.push_back(ChatMessage{"assistant", content, serialiseJsonValue(toolCallsValue), ""});

		ModelContext::ToolCallRound toolCallRound;

		toolCallRound.content = content;

		for(const rapidjson::Value& toolCallValue : toolCallsValue.GetArray())
		{
			// A result is paired with its call by id alone, so a call without one has nothing a result could
			// be given back against.
			std::string id = __stringMember(toolCallValue, "id");

			if(id.empty()) continue;

			std::string name;

			if(toolCallValue.HasMember("function"))
			{
				name = __stringMember(toolCallValue["function"], "name");
			}

			if(name.empty())
			{
				// Not recorded in the round, as a call with no name cannot be replayed. The model still
				// sees the error here, which is where it can act on it.
				messages.push_back(ChatMessage{"tool",
					jsonToolErrorContent("That tool call was not of a form that could be understood."),
					"", id});

				continue;
			}

			std::string argumentsText = __stringMember(toolCallValue["function"], "arguments");

			rapidjson::Document argumentsDocument;

			std::string arguments;

			std::string result;

			// A model that supplies no arguments may send an empty string rather than an empty object.
			if(argumentsText.empty())
			{
				result = processJsonToolCall(tools, name, 0);
			}
			else
			{
				argumentsDocument.Parse(argumentsText.c_str(), argumentsText.size());

				if(argumentsDocument.HasParseError() || !argumentsDocument.IsObject())
				{
					// Not invoked, as there are no arguments to give it, but reported so the model can
					// try again with ones that can be read.
					result = jsonToolErrorContent("The arguments of that tool call were not a JSON object.");
				}
				else
				{
					arguments = serialiseJsonValue(argumentsDocument);

					result = processJsonToolCall(tools, name, &argumentsDocument);
				}
			}

			messages.push_back(ChatMessage{"tool", result, "", id});

			toolCallRound.toolCalls.push_back(ModelContext::ToolCall{id, name, arguments, result});
		}

		// A round in which not one call could be attributed to a tool has nothing left worth replaying.
		if(!toolCallRound.toolCalls.empty()) toolCallRounds.push_back(toolCallRound);
	}

	throw AgentException(AgentException::TOOL_CALL_LIMIT_EXCEEDED);
}
