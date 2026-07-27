#include "OllamaModelProvider.hpp"

#include "AgentException.hpp"
#include "Model.hpp"
#include "ModelContext.hpp"
#include "ModelPrompt.hpp"
#include "ModelRequest.hpp"
#include "ModelSystemPrompt.hpp"
#include "ModelToolBindings.hpp"
#include "ModelToolDefinition.hpp"
#include "ModelToolDefinitionParameter.hpp"
#include "OllamaModel.hpp"
#include "modelToolJson.hpp"
#include "../rapidjson/document.h"

namespace
{
	// -- Ollama's chat wire format, used only by _processRequest --

	/// One entry of the chat history accumulated across rounds of tool calls.
	struct ChatMessage
	{
		/// Role the message is attributed to, i.e. "system", "user", "assistant" or "tool".
		std::string role;
		std::string content;

		/// Serialised tool_calls array echoed back to the model. Empty when the message had none.
		std::string toolCalls;

		/// Name of the tool this message carries the result of. Empty when not a tool result.
		std::string toolName;
	};

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
				// Ollama pairs a result with its call by name, so an id is only worth echoing back when the
				// server that recorded the call issued one.
				if(!toolCall.id.empty())
				{
					writer.Key("id");
					writeJsonString(writer, toolCall.id);
				}

				writer.Key("function");
				writer.StartObject();
					writer.Key("name");
					writeJsonString(writer, toolCall.name);

					writer.Key("arguments");

					if(toolCall.arguments.empty())
					{
						writer.StartObject();
						writer.EndObject();
					}
					else
					{
						writer.RawValue(toolCall.arguments.c_str(), toolCall.arguments.size(),
							rapidjson::kObjectType);
					}
				writer.EndObject();
			writer.EndObject();
		}

		writer.EndArray();

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	void __writeToolDefinition(ModelJsonWriter& writer, ModelToolDefinition& definition)
	{
		ModelToolDefinitionParameter returnType = definition.getReturnType();

		// Ollama's schema has nowhere to declare a return type, so fold it into the description rather
		// than dropping what the tool says it hands back.
		std::string description = definition.getDescription() + " Returns " + returnType.getName() + ": "
			+ returnType.getDescription();

		writer.StartObject();
			writer.Key("type");
			writer.String("function");

			writer.Key("function");
			writer.StartObject();
				writer.Key("name");
				writeJsonString(writer, definition.getName());

				writer.Key("description");
				writeJsonString(writer, description);

				writer.Key("parameters");
				writeJsonToolParametersSchema(writer, definition);
			writer.EndObject();
		writer.EndObject();
	}

	std::string __buildRequestBody(const std::string& modelName, std::vector<ChatMessage>& messages,
		std::vector<Handle<ModelToolBindings>>& tools, double temperature)
	{
		rapidjson::StringBuffer buffer;

		ModelJsonWriter writer(buffer);

		writer.StartObject();
			writer.Key("model");
			writeJsonString(writer, modelName);

			// The blocking POST cannot make sense of a stream of partial responses.
			writer.Key("stream");
			writer.Bool(false);

			writer.Key("messages");
			writer.StartArray();

			for(ChatMessage& message : messages)
			{
				writer.StartObject();
					writer.Key("role");
					writeJsonString(writer, message.role);

					writer.Key("content");
					writeJsonString(writer, message.content);

					if(!message.toolName.empty())
					{
						// Ollama correlates a result with its call by name; there is no OpenAI style id.
						writer.Key("tool_name");
						writeJsonString(writer, message.toolName);
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
						__writeToolDefinition(writer, definition);
					}
				}

				writer.EndArray();
			}

			// Left off entirely where no temperature was asked for, so that the server's own default
			// stands rather than being overwritten with a guess at what it is.
			if(temperature != ModelRequest::PROVIDER_DEFAULT_TEMPERATURE)
			{
				writer.Key("options");
				writer.StartObject();
					writer.Key("temperature");
					writeJsonTemperature(writer, temperature);
				writer.EndObject();
			}
		writer.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize());
	}
}

OllamaModelProvider::~OllamaModelProvider()
{
}

OllamaModelProvider::OllamaModelProvider(std::string url) : _url{url}
{
	_checkConnection(_url);

	_populateModels();
}

void OllamaModelProvider::_populateModels()
{
	std::string body = _httpGet(_url + "/api/tags");

	rapidjson::Document document;

	document.Parse(body.c_str());

	if(document.HasParseError() || !document.HasMember("models") || !document["models"].IsArray())
	{
		throw AgentException(AgentException::MODEL_FETCH_FAILED);
	}

	for(const rapidjson::Value& modelValue : document["models"].GetArray())
	{
		if(!modelValue.IsObject() || !modelValue.HasMember("name") || !modelValue["name"].IsString())
		{
			throw AgentException(AgentException::MODEL_FETCH_FAILED);
		}

		std::string description;

		if(modelValue.HasMember("details") && modelValue["details"].IsObject())
		{
			const rapidjson::Value& detailsValue = modelValue["details"];

			if(detailsValue.HasMember("family") && detailsValue["family"].IsString())
			{
				description += detailsValue["family"].GetString();
			}

			if(detailsValue.HasMember("parameter_size") && detailsValue["parameter_size"].IsString())
			{
				if(!description.empty()) description += " ";

				description += detailsValue["parameter_size"].GetString();
			}
		}

		// Ollama serves locally hosted models; there is no per-token cost.
		OllamaModel* model = new OllamaModel(this, modelValue["name"].GetString(), description, "0", "0", true);

		_addModel(Handle<Model>(model));

		// _addModel takes its own ref via the Handle; release the implicit construction ref.
		model -> decrRef();
	}
}

std::string OllamaModelProvider::_processRequest(Handle<Model> model, ModelRequest& request,
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
				messages.push_back(ChatMessage{"tool", toolCall.result, "", toolCall.name});
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
		std::string body = _httpPost(_url + "/api/chat",
			__buildRequestBody(modelName, messages, tools, temperature));

		rapidjson::Document document;

		document.Parse(body.c_str());

		// Ollama reports its own failures, e.g. an unknown model, as a top level "error".
		if(document.HasParseError() || !document.IsObject() || document.HasMember("error")
			|| !document.HasMember("message") || !document["message"].IsObject())
		{
			throw AgentException(AgentException::MODEL_REQUEST_FAILED);
		}

		const rapidjson::Value& messageValue = document["message"];

		std::string content;

		if(messageValue.HasMember("content") && messageValue["content"].IsString())
		{
			content = std::string(messageValue["content"].GetString(),
				messageValue["content"].GetStringLength());
		}

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
			if(!toolCallValue.IsObject() || !toolCallValue.HasMember("function")
				|| !toolCallValue["function"].IsObject() || !toolCallValue["function"].HasMember("name")
				|| !toolCallValue["function"]["name"].IsString())
			{
				// Not recorded in the round, as a call with no name to attribute a result to cannot be
				// replayed. The model still sees the error here, which is where it can act on it.
				messages.push_back(ChatMessage{"tool",
					jsonToolErrorContent("That tool call was not of a form that could be understood."),
					"", ""});

				continue;
			}

			const rapidjson::Value& functionValue = toolCallValue["function"];

			std::string name(functionValue["name"].GetString(), functionValue["name"].GetStringLength());

			std::string id;

			// Ollama's own chat endpoint correlates by name and sends no id, but an OpenAI compatible one
			// in front of it does, and a recorded call is worth keeping as it was made.
			if(toolCallValue.HasMember("id") && toolCallValue["id"].IsString())
			{
				id = std::string(toolCallValue["id"].GetString(), toolCallValue["id"].GetStringLength());
			}

			const rapidjson::Value* argumentsValue = 0;

			std::string arguments;

			if(functionValue.HasMember("arguments") && functionValue["arguments"].IsObject())
			{
				argumentsValue = &functionValue["arguments"];

				arguments = serialiseJsonValue(*argumentsValue);
			}

			std::string result = processJsonToolCall(tools, name, argumentsValue);

			messages.push_back(ChatMessage{"tool", result, "", name});

			toolCallRound.toolCalls.push_back(ModelContext::ToolCall{id, name, arguments, result});
		}

		// A round in which not one call could be attributed to a tool has nothing left worth replaying.
		if(!toolCallRound.toolCalls.empty()) toolCallRounds.push_back(toolCallRound);
	}

	throw AgentException(AgentException::TOOL_CALL_LIMIT_EXCEEDED);
}
