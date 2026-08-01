#ifndef HARNESS_LOADER_H
#define HARNESS_LOADER_H

#include "HarnessAssignmentDescriptor.hpp"
#include "HarnessProviderDescriptor.hpp"

/**
 * Format-agnostic supplier of the data needed to build an agentic harness: the providers models are
 * drawn from, and the models, system prompts and tool bindings assigned to each role and capability.
 * HarnessBuilder drives construction of an AgenticHarness entirely through this interface, so a new
 * persisted format only needs a new HarnessLoader subclass, never a change to HarnessBuilder itself.
 * @note Kept apart from HiveLoader because a harness is not part of a hive's structure: the same
 *       harness serves whichever hive it is given to, and a hive can run with no harness at all.
 * @note Entries are exposed by index rather than as a stream because HarnessBuilder needs every
 *       provider to exist before it can resolve the model assignments that name them.
 */
class HarnessLoader
{
	public:

		virtual ~HarnessLoader() {}

		/**
		 * Get the number of model providers in the harness being loaded.
		 */
		virtual unsigned getProviderCount() = 0;

		/**
		 * Get the descriptor of a single model provider.
		 * @param index Index of the provider, in [0, getProviderCount()).
		 */
		virtual HarnessProviderDescriptor getProvider(unsigned index) = 0;

		/**
		 * Get the number of model assignments in the harness being loaded.
		 */
		virtual unsigned getModelAssignmentCount() = 0;

		/**
		 * Get the descriptor of a single model assignment.
		 * @param index Index of the assignment, in [0, getModelAssignmentCount()).
		 */
		virtual HarnessModelAssignmentDescriptor getModelAssignment(unsigned index) = 0;

		/**
		 * Get the number of system prompt assignments in the harness being loaded.
		 */
		virtual unsigned getSystemPromptCount() = 0;

		/**
		 * Get the descriptor of a single system prompt assignment.
		 * @param index Index of the assignment, in [0, getSystemPromptCount()).
		 */
		virtual HarnessSystemPromptDescriptor getSystemPrompt(unsigned index) = 0;

		/**
		 * Get the number of tool binding assignments in the harness being loaded.
		 */
		virtual unsigned getToolBindingCount() = 0;

		/**
		 * Get the descriptor of a single tool binding assignment.
		 * @param index Index of the assignment, in [0, getToolBindingCount()).
		 */
		virtual HarnessToolBindingDescriptor getToolBinding(unsigned index) = 0;
};

#endif
