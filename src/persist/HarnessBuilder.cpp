#include "HarnessBuilder.hpp"

#include "HarnessAssignmentDescriptor.hpp"
#include "HarnessLoader.hpp"
#include "HarnessProviderDescriptor.hpp"
#include "PersistException.hpp"
#include "../agent/AgentException.hpp"
#include "../agent/Model.hpp"
#include "../agent/ModelProvider.hpp"
#include "../agent/ModelSystemPrompt.hpp"
#include "../agent/ModelToolBindings.hpp"
#include "../agent/OllamaModelProvider.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphToolBindingsFactory.hpp"

#include <map>

AgenticHarness* HarnessBuilder::build(HarnessLoader& loader, Handle<GraphHive> hive,
	Handle<GraphToolBindingsFactory> toolBindingsFactory)
{
	unsigned providerCount = loader.getProviderCount();

	if(providerCount == 0)
	{
		throw PersistException(PersistException::NO_MODEL_PROVIDERS);
	}

	unsigned modelAssignmentCount = loader.getModelAssignmentCount();

	if(modelAssignmentCount == 0)
	{
		throw PersistException(PersistException::NO_MODEL_ASSIGNMENTS);
	}

	std::map<std::string, Handle<ModelProvider>> providersByName;

	// -- Pass 1: connect to every provider, indexed by name --
	// Done before the harness exists so that an unreachable server, which is the one failure here that
	// has nothing to do with the file being read, is reported without anything having been built.
	for(unsigned i = 0; i < providerCount; i++)
	{
		HarnessProviderDescriptor descriptor = loader.getProvider(i);

		if(descriptor.name.empty())
		{
			throw PersistException(PersistException::INVALID_PROVIDER_NAME);
		}

		if(providersByName.find(descriptor.name) != providersByName.end())
		{
			throw PersistException(PersistException::DUPLICATE_PROVIDER_NAME);
		}

		providersByName.emplace(descriptor.name, __createProvider(descriptor));
	}

	AgenticHarness* harness = new AgenticHarness();

	try
	{
		// -- Pass 2: assign models (needs every provider to already exist by name) --
		for(unsigned i = 0; i < modelAssignmentCount; i++)
		{
			HarnessModelAssignmentDescriptor descriptor = loader.getModelAssignment(i);

			auto providerIt = providersByName.find(descriptor.providerName);

			if(providerIt == providersByName.end())
			{
				throw PersistException(PersistException::MODEL_PROVIDER_NOT_FOUND);
			}

			Handle<Model> modelHandle(0);

			for(Handle<Model>& candidate : providerIt -> second.getInstance() -> getModels())
			{
				if(candidate.getInstance() -> getName() != descriptor.modelName) continue;

				modelHandle = candidate;

				break;
			}

			if(!modelHandle.isValid())
			{
				throw PersistException(PersistException::MODEL_NOT_FOUND);
			}

			AgenticHarness::RoleCapability roleCapability{
				__roleFromName(descriptor.roleCapability.roleName),
				__capabilityFromName(descriptor.roleCapability.capabilityName)};

			try
			{
				if(descriptor.hasTemperature)
				{
					harness -> addModelAssignment(roleCapability, modelHandle, descriptor.temperature);
				}
				else
				{
					harness -> addModelAssignment(roleCapability, modelHandle);
				}
			}
			catch(AgentException&)
			{
				// The only fault the assignment itself refuses. Re-raised as a persist error so that a
				// caller loading a harness deals with one exception type.
				throw PersistException(PersistException::INVALID_MODEL_TEMPERATURE);
			}
		}

		// -- Pass 3: assign system prompts --
		unsigned systemPromptCount = loader.getSystemPromptCount();

		for(unsigned i = 0; i < systemPromptCount; i++)
		{
			HarnessSystemPromptDescriptor descriptor = loader.getSystemPrompt(i);

			if(descriptor.prompt.empty())
			{
				throw PersistException(PersistException::EMPTY_SYSTEM_PROMPT);
			}

			AgenticHarness::RoleCapability roleCapability{
				__roleFromName(descriptor.roleCapability.roleName),
				__capabilityFromName(descriptor.roleCapability.capabilityName)};

			harness -> addSystemPrompt(roleCapability, ModelSystemPrompt(descriptor.prompt));
		}

		// -- Pass 4: assign tool bindings --
		unsigned toolBindingCount = loader.getToolBindingCount();

		if(toolBindingCount > 0 && (!hive.isValid() || !toolBindingsFactory.isValid()))
		{
			throw PersistException(PersistException::HARNESS_HIVE_NOT_AVAILABLE);
		}

		for(unsigned i = 0; i < toolBindingCount; i++)
		{
			HarnessToolBindingDescriptor descriptor = loader.getToolBinding(i);

			if(descriptor.roleCapabilities.empty())
			{
				throw PersistException(PersistException::TOOL_BINDING_WITHOUT_ROLE);
			}

			// Built once per pair rather than once per entry, as the factory supplies a set for one
			// capability at a time and an entry may name several. Each pair is then assigned on its own,
			// which a request only ever matches one of, so the model behind it sees the tools once
			// however many pairs the entry listed.
			for(HarnessRoleCapabilityDescriptor& roleCapabilityDescriptor : descriptor.roleCapabilities)
			{
				AgenticHarness::RoleCapability roleCapability{
					__roleFromName(roleCapabilityDescriptor.roleName),
					__capabilityFromName(roleCapabilityDescriptor.capabilityName)};

				std::vector<Handle<ModelToolBindings>> tools = __toolBindingsFromName(descriptor.toolSetName,
					roleCapability.capability, hive, toolBindingsFactory);

				for(Handle<ModelToolBindings>& tool : tools)
				{
					harness -> addToolBinding({roleCapability}, tool);
				}
			}
		}
	}
	catch(...)
	{
		// Drops the harness's construction reference (nothing else holds one at this point), deleting it.
		harness -> decrRef();
		throw;
	}

	return harness;
}

Handle<ModelProvider> HarnessBuilder::__createProvider(const HarnessProviderDescriptor& descriptor)
{
	if(descriptor.url.empty())
	{
		throw PersistException(PersistException::INVALID_PROVIDER_URL);
	}

	switch(descriptor.type)
	{
		case HarnessProviderDescriptor::OLLAMA:
		{
			try
			{
				OllamaModelProvider* provider = new OllamaModelProvider(descriptor.url);

				Handle<ModelProvider> providerHandle(provider);

				// The handle carries the reference out to the caller; release the implicit construction ref.
				provider -> decrRef();

				return providerHandle;
			}
			catch(AgentException&)
			{
				// Covers both the server being unreachable and it answering with a model list that could
				// not be read; either way there is no provider to assign models from.
				throw PersistException(PersistException::PROVIDER_CONNECTION_FAILED);
			}
		}
	}

	throw PersistException(PersistException::UNKNOWN_PROVIDER_TYPE);
}

std::vector<Handle<ModelToolBindings>> HarnessBuilder::__toolBindingsFromName(const std::string& toolSetName,
	AgenticHarness::Capability capability, Handle<GraphHive> hive,
	Handle<GraphToolBindingsFactory> toolBindingsFactory)
{
	GraphToolBindingsFactory* factory = toolBindingsFactory.getInstance();

	if(toolSetName == "HIVE") return factory -> getHiveToolBindings(capability, hive);
	if(toolSetName == "CHAT") return factory -> getChatToolBindings(capability, hive);

	throw PersistException(PersistException::UNKNOWN_TOOL_BINDING_SET);
}

AgenticHarness::Role HarnessBuilder::__roleFromName(const std::string& name)
{
	if(name == "CHAT") return AgenticHarness::Role::CHAT;
	if(name == "HIVE") return AgenticHarness::Role::HIVE;
	if(name == "NODE") return AgenticHarness::Role::NODE;
	if(name == "SCRIPT") return AgenticHarness::Role::SCRIPT;

	throw PersistException(PersistException::UNKNOWN_AGENT_ROLE);
}

AgenticHarness::Capability HarnessBuilder::__capabilityFromName(const std::string& name)
{
	if(name == "LOW") return AgenticHarness::Capability::LOW;
	if(name == "MEDIUM") return AgenticHarness::Capability::MEDIUM;
	if(name == "HIGH") return AgenticHarness::Capability::HIGH;

	throw PersistException(PersistException::UNKNOWN_AGENT_CAPABILITY);
}
