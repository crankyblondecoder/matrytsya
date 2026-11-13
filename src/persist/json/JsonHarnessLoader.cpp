#include "JsonHarnessLoader.hpp"

#include "../PersistException.hpp"
#include "../../agent/OpenRouterModelProvider.hpp"
#include "../../rapidjson/document.h"
#include "../../rapidjson/error/en.h"

#include <algorithm>

namespace
{
	// -- Parsing helpers, used only while JsonHarnessLoader's constructor extracts everything up front --

	HarnessProviderDescriptor::Type __providerTypeFromString(const std::string& type)
	{
		if(type == "Ollama") return HarnessProviderDescriptor::OLLAMA;
		if(type == "OpenRouter") return HarnessProviderDescriptor::OPENROUTER;

		throw PersistException(PersistException::UNKNOWN_PROVIDER_TYPE);
	}

	HarnessProviderDescriptor __parseProvider(const rapidjson::Value& providerValue)
	{
		if(!providerValue.IsObject() || !providerValue.HasMember("type") || !providerValue["type"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_PROVIDER);
		}

		HarnessProviderDescriptor descriptor{};

		// May throw UNKNOWN_PROVIDER_TYPE if "type" is a string but not one of the recognised values.
		descriptor.type = __providerTypeFromString(providerValue["type"].GetString());

		if(!providerValue.HasMember("name") || !providerValue["name"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_PROVIDER);
		}

		descriptor.name = providerValue["name"].GetString();

		if(providerValue.HasMember("url"))
		{
			if(!providerValue["url"].IsString()) throw PersistException(PersistException::JSON_INVALID_PROVIDER);

			descriptor.url = providerValue["url"].GetString();
		}

		switch(descriptor.type)
		{
			case HarnessProviderDescriptor::OLLAMA:

				// An Ollama server lives wherever it was set up, so there is no address to fall back on.
				if(!providerValue.HasMember("url")) throw PersistException(PersistException::JSON_INVALID_PROVIDER);

				break;

			case HarnessProviderDescriptor::OPENROUTER:

				if(!providerValue.HasMember("url")) descriptor.url = OpenRouterModelProvider::DEFAULT_URL;

				if(!providerValue.HasMember("apiKeyEnv") || !providerValue["apiKeyEnv"].IsString())
				{
					throw PersistException(PersistException::JSON_INVALID_PROVIDER);
				}

				descriptor.apiKeyEnv = providerValue["apiKeyEnv"].GetString();

				break;
		}

		return descriptor;
	}

	HarnessRoleCapabilityDescriptor __parseRoleCapability(const rapidjson::Value& parentValue)
	{
		if(!parentValue.HasMember("roleCapability")) throw PersistException(PersistException::JSON_INVALID_ROLE_CAPABILITY);

		const rapidjson::Value& roleCapabilityValue = parentValue["roleCapability"];

		if(!roleCapabilityValue.IsObject() ||
			!roleCapabilityValue.HasMember("role") || !roleCapabilityValue["role"].IsString() ||
			!roleCapabilityValue.HasMember("capability") || !roleCapabilityValue["capability"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_ROLE_CAPABILITY);
		}

		HarnessRoleCapabilityDescriptor descriptor;

		// Only checked for being names here; whether they are names that exist is HarnessBuilder's concern.
		descriptor.roleName = roleCapabilityValue["role"].GetString();
		descriptor.capabilityName = roleCapabilityValue["capability"].GetString();

		return descriptor;
	}

	HarnessModelAssignmentDescriptor __parseModelAssignment(const rapidjson::Value& assignmentValue)
	{
		if(!assignmentValue.IsObject()) throw PersistException(PersistException::JSON_INVALID_MODEL_ASSIGNMENTS);

		HarnessModelAssignmentDescriptor descriptor{};

		descriptor.roleCapability = __parseRoleCapability(assignmentValue);

		if(!assignmentValue.HasMember("model")) throw PersistException(PersistException::JSON_INVALID_MODEL_REFERENCE);

		const rapidjson::Value& modelValue = assignmentValue["model"];

		if(!modelValue.IsObject() ||
			!modelValue.HasMember("providerName") || !modelValue["providerName"].IsString() ||
			!modelValue.HasMember("modelName") || !modelValue["modelName"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_MODEL_REFERENCE);
		}

		descriptor.providerName = modelValue["providerName"].GetString();
		descriptor.modelName = modelValue["modelName"].GetString();

		// Absence is meaningful rather than a default to fill in: it is what asks for no temperature at
		// all, leaving the choice to the provider.
		if(assignmentValue.HasMember("temperature"))
		{
			if(!assignmentValue["temperature"].IsNumber())
			{
				throw PersistException(PersistException::JSON_INVALID_MODEL_TEMPERATURE);
			}

			descriptor.hasTemperature = true;
			descriptor.temperature = assignmentValue["temperature"].GetDouble();
		}

		return descriptor;
	}

	HarnessSystemPromptDescriptor __parseSystemPrompt(const rapidjson::Value& systemPromptValue)
	{
		if(!systemPromptValue.IsObject()) throw PersistException(PersistException::JSON_INVALID_SYSTEM_PROMPTS);

		HarnessSystemPromptDescriptor descriptor{};

		descriptor.roleCapability = __parseRoleCapability(systemPromptValue);

		if(!systemPromptValue.HasMember("prompt") || !systemPromptValue["prompt"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_SYSTEM_PROMPTS);
		}

		descriptor.prompt = systemPromptValue["prompt"].GetString();

		return descriptor;
	}

	HarnessToolBindingDescriptor __parseToolBinding(const rapidjson::Value& toolBindingValue)
	{
		if(!toolBindingValue.IsObject() ||
			!toolBindingValue.HasMember("roleCapabilities") || !toolBindingValue["roleCapabilities"].IsArray() ||
			!toolBindingValue.HasMember("tools") || !toolBindingValue["tools"].IsString())
		{
			throw PersistException(PersistException::JSON_INVALID_TOOL_BINDINGS);
		}

		HarnessToolBindingDescriptor descriptor{};

		for(auto& roleCapabilityValue : toolBindingValue["roleCapabilities"].GetArray())
		{
			if(!roleCapabilityValue.IsObject() ||
				!roleCapabilityValue.HasMember("role") || !roleCapabilityValue["role"].IsString() ||
				!roleCapabilityValue.HasMember("capability") || !roleCapabilityValue["capability"].IsString())
			{
				throw PersistException(PersistException::JSON_INVALID_ROLE_CAPABILITY);
			}

			HarnessRoleCapabilityDescriptor roleCapabilityDescriptor;

			roleCapabilityDescriptor.roleName = roleCapabilityValue["role"].GetString();
			roleCapabilityDescriptor.capabilityName = roleCapabilityValue["capability"].GetString();

			descriptor.roleCapabilities.push_back(roleCapabilityDescriptor);
		}

		// Only checked for being a name here; whether it is a name the factory supplies is HarnessBuilder's
		// concern.
		descriptor.toolSetName = toolBindingValue["tools"].GetString();

		return descriptor;
	}
}

JsonHarnessLoader::JsonHarnessLoader(const std::string& json)
{
	rapidjson::Document document;

	document.Parse(json.c_str());

	if(document.HasParseError())
	{
		// Given as a line and column rather than the byte offset reported, as that is what an editor shows.
		size_t offset = document.GetErrorOffset();
		size_t line = 1 + std::count(json.begin(), json.begin() + offset, '\n');
		size_t lineStart = offset == 0 ? std::string::npos : json.rfind('\n', offset - 1);
		size_t column = 1 + offset - (lineStart == std::string::npos ? 0 : lineStart + 1);

		throw PersistException(PersistException::JSON_PARSE_ERROR, "line " + std::to_string(line) +
			", column " + std::to_string(column) + ": " + rapidjson::GetParseError_En(document.GetParseError()));
	}

	if(!document.IsObject()) throw PersistException(PersistException::JSON_ROOT_NOT_OBJECT);

	if(!document.HasMember("providers") || !document["providers"].IsArray())
	{
		throw PersistException(PersistException::JSON_INVALID_PROVIDERS);
	}

	// An empty "providers" array is accepted here; requiring at least one provider is HarnessBuilder's
	// concern, as is every other structural rule that spans more than one entry.
	for(auto& providerValue : document["providers"].GetArray())
	{
		_providers.push_back(__parseProvider(providerValue));
	}

	if(!document.HasMember("modelAssignments") || !document["modelAssignments"].IsArray())
	{
		throw PersistException(PersistException::JSON_INVALID_MODEL_ASSIGNMENTS);
	}

	for(auto& assignmentValue : document["modelAssignments"].GetArray())
	{
		_modelAssignments.push_back(__parseModelAssignment(assignmentValue));
	}

	if(document.HasMember("systemPrompts"))
	{
		const rapidjson::Value& systemPromptsValue = document["systemPrompts"];

		if(!systemPromptsValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_SYSTEM_PROMPTS);

		for(auto& systemPromptValue : systemPromptsValue.GetArray())
		{
			_systemPrompts.push_back(__parseSystemPrompt(systemPromptValue));
		}
	}

	if(document.HasMember("toolBindings"))
	{
		const rapidjson::Value& toolBindingsValue = document["toolBindings"];

		if(!toolBindingsValue.IsArray()) throw PersistException(PersistException::JSON_INVALID_TOOL_BINDINGS);

		for(auto& toolBindingValue : toolBindingsValue.GetArray())
		{
			_toolBindings.push_back(__parseToolBinding(toolBindingValue));
		}
	}
}

JsonHarnessLoader::~JsonHarnessLoader()
{
}

unsigned JsonHarnessLoader::getProviderCount()
{
	return _providers.size();
}

HarnessProviderDescriptor JsonHarnessLoader::getProvider(unsigned index)
{
	return _providers[index];
}

unsigned JsonHarnessLoader::getModelAssignmentCount()
{
	return _modelAssignments.size();
}

HarnessModelAssignmentDescriptor JsonHarnessLoader::getModelAssignment(unsigned index)
{
	return _modelAssignments[index];
}

unsigned JsonHarnessLoader::getSystemPromptCount()
{
	return _systemPrompts.size();
}

HarnessSystemPromptDescriptor JsonHarnessLoader::getSystemPrompt(unsigned index)
{
	return _systemPrompts[index];
}

unsigned JsonHarnessLoader::getToolBindingCount()
{
	return _toolBindings.size();
}

HarnessToolBindingDescriptor JsonHarnessLoader::getToolBinding(unsigned index)
{
	return _toolBindings[index];
}
