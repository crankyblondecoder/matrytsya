#ifndef JSON_HARNESS_LOADER_UNIT_TEST_H
#define JSON_HARNESS_LOADER_UNIT_TEST_H

#include <gtest/gtest.h>

#include "../../../persist/HarnessAssignmentDescriptor.hpp"
#include "../../../persist/HarnessProviderDescriptor.hpp"
#include "../../../persist/PersistException.hpp"
#include "../../../persist/json/JsonHarnessLoader.hpp"

#include <string>

/**
 * A JSON document covering a provider, both kinds of assignment and a tool binding parses into
 * descriptors with the expected fields.
 */
TEST(JsonHarnessLoaderTest, FullValidHarness_AllSectionsParsed)
{
	std::string json = R"({
		"providers": [
			{ "type": "Ollama", "name": "localOllama", "url": "http://localhost:11434" }
		],
		"modelAssignments": [
			{
				"roleCapability": { "role": "CHAT", "capability": "LOW" },
				"model": { "providerName": "localOllama", "modelName": "someModel:30b" },
				"temperature": 0.2
			},
			{
				"roleCapability": { "role": "NODE", "capability": "HIGH" },
				"model": { "providerName": "localOllama", "modelName": "someModel:30b" }
			}
		],
		"systemPrompts": [
			{ "roleCapability": { "role": "CHAT", "capability": "LOW" }, "prompt": "you are a chat assistant" }
		],
		"toolBindings": [
			{
				"roleCapabilities": [
					{ "role": "CHAT", "capability": "LOW" },
					{ "role": "HIVE", "capability": "HIGH" }
				],
				"tools": "CHAT"
			}
		]
	})";

	JsonHarnessLoader loader(json);

	ASSERT_EQ(loader.getProviderCount(), 1u);

	HarnessProviderDescriptor provider = loader.getProvider(0);
	EXPECT_EQ(provider.type, HarnessProviderDescriptor::OLLAMA);
	EXPECT_EQ(provider.name, "localOllama");
	EXPECT_EQ(provider.url, "http://localhost:11434");

	ASSERT_EQ(loader.getModelAssignmentCount(), 2u);

	HarnessModelAssignmentDescriptor chatAssignment = loader.getModelAssignment(0);
	EXPECT_EQ(chatAssignment.roleCapability.roleName, "CHAT");
	EXPECT_EQ(chatAssignment.roleCapability.capabilityName, "LOW");
	EXPECT_EQ(chatAssignment.providerName, "localOllama");
	EXPECT_EQ(chatAssignment.modelName, "someModel:30b");
	EXPECT_TRUE(chatAssignment.hasTemperature);
	EXPECT_DOUBLE_EQ(chatAssignment.temperature, 0.2);

	// An assignment with no "temperature" asks for none at all, rather than for some default value.
	HarnessModelAssignmentDescriptor nodeAssignment = loader.getModelAssignment(1);
	EXPECT_EQ(nodeAssignment.roleCapability.roleName, "NODE");
	EXPECT_EQ(nodeAssignment.roleCapability.capabilityName, "HIGH");
	EXPECT_FALSE(nodeAssignment.hasTemperature);

	ASSERT_EQ(loader.getSystemPromptCount(), 1u);

	HarnessSystemPromptDescriptor systemPrompt = loader.getSystemPrompt(0);
	EXPECT_EQ(systemPrompt.roleCapability.roleName, "CHAT");
	EXPECT_EQ(systemPrompt.roleCapability.capabilityName, "LOW");
	EXPECT_EQ(systemPrompt.prompt, "you are a chat assistant");

	ASSERT_EQ(loader.getToolBindingCount(), 1u);

	HarnessToolBindingDescriptor toolBinding = loader.getToolBinding(0);
	EXPECT_EQ(toolBinding.toolSetName, "CHAT");
	ASSERT_EQ(toolBinding.roleCapabilities.size(), 2u);
	EXPECT_EQ(toolBinding.roleCapabilities[0].roleName, "CHAT");
	EXPECT_EQ(toolBinding.roleCapabilities[0].capabilityName, "LOW");
	EXPECT_EQ(toolBinding.roleCapabilities[1].roleName, "HIVE");
	EXPECT_EQ(toolBinding.roleCapabilities[1].capabilityName, "HIGH");
}

/**
 * The optional sections may be left out entirely, leaving a harness whose roles are served by a model
 * with no system prompt and no tools.
 */
TEST(JsonHarnessLoaderTest, OptionalSectionsOmitted_ParseAsEmpty)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{
				"roleCapability": { "role": "SCRIPT", "capability": "MEDIUM" },
				"model": { "providerName": "p", "modelName": "m" }
			}
		]
	})";

	JsonHarnessLoader loader(json);

	EXPECT_EQ(loader.getSystemPromptCount(), 0u);
	EXPECT_EQ(loader.getToolBindingCount(), 0u);
}

/**
 * Text that is not JSON at all is rejected.
 */
TEST(JsonHarnessLoaderTest, MalformedJson_ThrowsJsonParseError)
{
	std::string json = R"({ "providers": [ )";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * The "providers" member is required, as a harness with nothing to draw models from can serve nothing.
 */
TEST(JsonHarnessLoaderTest, MissingProviders_ThrowsJsonInvalidProviders)
{
	std::string json = R"({ "modelAssignments": [] })";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * A provider "type" string that doesn't match any known concrete provider type is rejected.
 */
TEST(JsonHarnessLoaderTest, UnrecognisedProviderType_ThrowsUnknownProviderType)
{
	std::string json = R"({
		"providers": [ { "type": "NotARealProvider", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": []
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * A provider missing its required "url" is rejected.
 */
TEST(JsonHarnessLoaderTest, ProviderMissingUrl_ThrowsJsonInvalidProvider)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p" } ],
		"modelAssignments": []
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * The "modelAssignments" member is required, as a harness with no model behind any role can service no
 * request.
 */
TEST(JsonHarnessLoaderTest, MissingModelAssignments_ThrowsJsonInvalidModelAssignments)
{
	std::string json = R"({ "providers": [] })";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * An assignment whose "roleCapability" names only a role is rejected: both halves are needed, as
 * assignments are matched on the pair.
 */
TEST(JsonHarnessLoaderTest, RoleCapabilityMissingCapability_ThrowsJsonInvalidRoleCapability)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{ "roleCapability": { "role": "CHAT" }, "model": { "providerName": "p", "modelName": "m" } }
		]
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * An assignment whose "model" does not name both the provider and the model is rejected.
 */
TEST(JsonHarnessLoaderTest, ModelReferenceMissingModelName_ThrowsJsonInvalidModelReference)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{ "roleCapability": { "role": "CHAT", "capability": "LOW" }, "model": { "providerName": "p" } }
		]
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * A "temperature" that is not a number is rejected. Whether a number is one a request permits is
 * HarnessBuilder's concern.
 */
TEST(JsonHarnessLoaderTest, NonNumericTemperature_ThrowsJsonInvalidModelTemperature)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{
				"roleCapability": { "role": "CHAT", "capability": "LOW" },
				"model": { "providerName": "p", "modelName": "m" },
				"temperature": "warm"
			}
		]
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * A tool binding entry missing its "tools" member is rejected.
 */
TEST(JsonHarnessLoaderTest, ToolBindingMissingTools_ThrowsJsonInvalidToolBindings)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{ "roleCapability": { "role": "CHAT", "capability": "LOW" }, "model": { "providerName": "p", "modelName": "m" } }
		],
		"toolBindings": [ { "roleCapabilities": [ { "role": "CHAT", "capability": "LOW" } ] } ]
	})";

	EXPECT_THROW(JsonHarnessLoader loader(json), PersistException);
}

/**
 * A tool set name the factory may not supply is accepted by the loader itself; recognising the name is
 * HarnessBuilder's business rule, not JsonHarnessLoader's.
 */
TEST(JsonHarnessLoaderTest, UnrecognisedToolSetName_DoesNotThrowAtLoaderLevel)
{
	std::string json = R"({
		"providers": [ { "type": "Ollama", "name": "p", "url": "http://localhost:11434" } ],
		"modelAssignments": [
			{ "roleCapability": { "role": "CHAT", "capability": "LOW" }, "model": { "providerName": "p", "modelName": "m" } }
		],
		"toolBindings": [
			{ "roleCapabilities": [ { "role": "CHAT", "capability": "LOW" } ], "tools": "NOT_A_TOOL_SET" }
		]
	})";

	JsonHarnessLoader loader(json);

	ASSERT_EQ(loader.getToolBindingCount(), 1u);
	EXPECT_EQ(loader.getToolBinding(0).toolSetName, "NOT_A_TOOL_SET");
}

/**
 * An empty "providers" array is accepted by the loader itself; requiring at least one provider is
 * HarnessBuilder's business rule, not JsonHarnessLoader's.
 */
TEST(JsonHarnessLoaderTest, EmptyProvidersArray_DoesNotThrowAtLoaderLevel)
{
	std::string json = R"({ "providers": [], "modelAssignments": [] })";

	JsonHarnessLoader loader(json);

	EXPECT_EQ(loader.getProviderCount(), 0u);
	EXPECT_EQ(loader.getModelAssignmentCount(), 0u);
}

#endif
