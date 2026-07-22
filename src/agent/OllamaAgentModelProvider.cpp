#include "OllamaAgentModelProvider.hpp"

#include "AgentException.hpp"
#include "../rapidjson/document.h"

OllamaAgentModelProvider::~OllamaAgentModelProvider()
{
}

OllamaAgentModelProvider::OllamaAgentModelProvider(std::string url) : _url{url}
{
	_checkConnection(_url);

	_populateModels();
}

void OllamaAgentModelProvider::_populateModels()
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
		_addModel(AgentModel(this, modelValue["name"].GetString(), description, "0", "0", true));
	}
}
