#ifndef HARNESS_ASSIGNMENT_DESCRIPTOR_H
#define HARNESS_ASSIGNMENT_DESCRIPTOR_H

#include <string>
#include <vector>

/**
 * Describes the role, and the capability that role is served at, that a single harness assignment
 * applies to.
 * @note Both are carried as the names they were written under rather than as enums, so that a name that
 *       does not exist is reported by HarnessBuilder alongside every other structural fault, rather
 *       than by whichever loader happened to read it.
 */
struct HarnessRoleCapabilityDescriptor
{
	/// Name of the role, as it appears in AgenticHarness::Role (e.g. "CHAT").
	std::string roleName;

	/// Name of the capability, as it appears in AgenticHarness::Capability (e.g. "LOW").
	std::string capabilityName;
};

/**
 * Format-agnostic description of a single model assignment, as supplied by a HarnessLoader and consumed
 * by HarnessBuilder.
 */
struct HarnessModelAssignmentDescriptor
{
	/// Role, and capability of that role, this model answers requests for.
	HarnessRoleCapabilityDescriptor roleCapability;

	/// Name of the provider, within the same harness, that serves the model.
	std::string providerName;

	/// Name the provider serves the model under.
	std::string modelName;

	/// Whether temperature was supplied. When false, no temperature is asked for and the provider
	/// answers at whatever it defaults to.
	bool hasTemperature = false;

	/// Sampling temperature every request answered through this assignment is made at. Only meaningful
	/// when hasTemperature is true.
	double temperature = 0.0;
};

/**
 * Format-agnostic description of a single system prompt assignment, as supplied by a HarnessLoader and
 * consumed by HarnessBuilder.
 */
struct HarnessSystemPromptDescriptor
{
	/// Role, and capability of that role, this prompt is used for.
	HarnessRoleCapabilityDescriptor roleCapability;

	/// Text of the system prompt.
	std::string prompt;
};

/**
 * Format-agnostic description of a single tool binding assignment, as supplied by a HarnessLoader and
 * consumed by HarnessBuilder.
 */
struct HarnessToolBindingDescriptor
{
	/// Roles, and the capability of each, that may use the tools.
	std::vector<HarnessRoleCapabilityDescriptor> roleCapabilities;

	/// Name of the set of tools made available, as it is written in a harness definition file (e.g.
	/// "CHAT"). HarnessBuilder maps it onto the retrieval method of the hive's tool bindings factory
	/// that supplies it.
	std::string toolSetName;
};

#endif
