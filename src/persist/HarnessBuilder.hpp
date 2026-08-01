#ifndef HARNESS_BUILDER_H
#define HARNESS_BUILDER_H

#include <string>
#include <vector>

// Needed in full because the role/capability translation helpers return its enums.
#include "../agent/AgenticHarness.hpp"
#include "../util/Handle.hpp"

class GraphHive;
class GraphToolBindingsFactory;
class HarnessLoader;
class ModelProvider;
class ModelToolBindings;
struct HarnessProviderDescriptor;

/**
 * Builds a fully populated AgenticHarness from any HarnessLoader.
 * @note Format-agnostic: all harness data (providers, model assignments, system prompts, tool bindings)
 *       comes from the loader, so a new persisted format only needs a new HarnessLoader subclass, never
 *       a change here.
 * @note A harness is built separately from the hive it serves, rather than as part of HiveBuilder, so
 *       that which models a hive is run against can be changed without touching the hive definition,
 *       and so that a hive can be built and run with no harness at all.
 */
class HarnessBuilder
{
	public:

		/**
		 * Build a fully populated agentic harness from a loader.
		 * @param loader Loader supplying the harness's providers, model assignments, system prompts and
		 *        tool binding assignments.
		 * @param hive Hive the tool bindings are to operate against. Only needed where the loader
		 *        supplies tool binding assignments.
		 * @param toolBindingsFactory Factory the tool bindings are taken from. Only needed where the
		 *        loader supplies tool binding assignments. Passed in rather than read back off the hive
		 *        so that a harness cannot end up with no tools purely because of the order the hive's
		 *        factory and harness were set in.
		 * @returns Newly allocated, fully populated AgenticHarness. Caller takes ownership of the
		 *          initial reference.
		 * @throw PersistException On any structural problem in the loader's data, and on a provider that
		 *        cannot be reached. Every provider named is contacted while building, so this blocks for
		 *        as long as those servers take to answer.
		 */
		static AgenticHarness* build(HarnessLoader& loader, Handle<GraphHive> hive,
			Handle<GraphToolBindingsFactory> toolBindingsFactory);

	private:

		// Not instantiable.
		HarnessBuilder();
		HarnessBuilder(const HarnessBuilder& copyFrom);
		HarnessBuilder& operator= (const HarnessBuilder& copyFrom);

		/**
		 * Create and connect the concrete ModelProvider subclass described by a descriptor.
		 * @param descriptor Descriptor of the provider to create.
		 * @returns Handle to the connected provider, with its models already populated.
		 * @throw PersistException(INVALID_PROVIDER_URL) If the descriptor's URL is empty.
		 * @throw PersistException(PROVIDER_CONNECTION_FAILED) If the server cannot be reached or will
		 *        not report the models it serves.
		 */
		static Handle<ModelProvider> __createProvider(const HarnessProviderDescriptor& descriptor);

		/**
		 * Get the set of tool bindings a name stands for, built against the hive for one capability.
		 * @param toolSetName Name of the set, as it is written in a harness definition file.
		 * @param capability Capability of the model the bindings are being built for.
		 * @param hive Hive the bindings are to operate against.
		 * @param toolBindingsFactory Factory the bindings are taken from.
		 * @returns The bindings built. Empty where the factory supplies none.
		 * @throw PersistException(UNKNOWN_TOOL_BINDING_SET) If the name is not one the factory supplies.
		 */
		static std::vector<Handle<ModelToolBindings>> __toolBindingsFromName(const std::string& toolSetName,
			AgenticHarness::Capability capability, Handle<GraphHive> hive,
			Handle<GraphToolBindingsFactory> toolBindingsFactory);

		/**
		 * Translate a role name into its enum value.
		 * @param name Role name, as it appears in AgenticHarness::Role.
		 * @throw PersistException(UNKNOWN_AGENT_ROLE) If the name is not recognised.
		 */
		static AgenticHarness::Role __roleFromName(const std::string& name);

		/**
		 * Translate a capability name into its enum value.
		 * @param name Capability name, as it appears in AgenticHarness::Capability.
		 * @throw PersistException(UNKNOWN_AGENT_CAPABILITY) If the name is not recognised.
		 */
		static AgenticHarness::Capability __capabilityFromName(const std::string& name);
};

#endif
