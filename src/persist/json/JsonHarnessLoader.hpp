#ifndef JSON_HARNESS_LOADER_H
#define JSON_HARNESS_LOADER_H

#include <string>
#include <vector>

#include "../HarnessAssignmentDescriptor.hpp"
#include "../HarnessLoader.hpp"
#include "../HarnessProviderDescriptor.hpp"

/**
 * HarnessLoader that parses a JSON string matching harnessSchema.json, using RapidJSON.
 * @note This loads JSON that matches the schema; it does not validate arbitrary JSON against the
 *       schema. Unrecognised/extra fields are silently ignored rather than rejected.
 */
class JsonHarnessLoader : public HarnessLoader
{
	public:

		/**
		 * Parses and validates json immediately; all harness data is extracted up front.
		 * @param json JSON text describing an agentic harness, per harnessSchema.json.
		 * @throw PersistException On any parse or structural validation failure.
		 */
		JsonHarnessLoader(const std::string& json);

		virtual ~JsonHarnessLoader();

		unsigned getProviderCount() override;
		HarnessProviderDescriptor getProvider(unsigned index) override;
		unsigned getModelAssignmentCount() override;
		HarnessModelAssignmentDescriptor getModelAssignment(unsigned index) override;
		unsigned getSystemPromptCount() override;
		HarnessSystemPromptDescriptor getSystemPrompt(unsigned index) override;
		unsigned getToolBindingCount() override;
		HarnessToolBindingDescriptor getToolBinding(unsigned index) override;

	private:

		// Do not allow copying.
		JsonHarnessLoader(const JsonHarnessLoader& copyFrom);
		JsonHarnessLoader& operator= (const JsonHarnessLoader& copyFrom);

		/// Descriptors of every provider, parsed from the top level "providers" array, in order.
		std::vector<HarnessProviderDescriptor> _providers;

		/// Descriptors of every model assignment, parsed from the top level "modelAssignments" array, in order.
		std::vector<HarnessModelAssignmentDescriptor> _modelAssignments;

		/// Descriptors of every system prompt assignment, parsed from the top level "systemPrompts" array, in order.
		std::vector<HarnessSystemPromptDescriptor> _systemPrompts;

		/// Descriptors of every tool binding assignment, parsed from the top level "toolBindings" array, in order.
		std::vector<HarnessToolBindingDescriptor> _toolBindings;
};

#endif
