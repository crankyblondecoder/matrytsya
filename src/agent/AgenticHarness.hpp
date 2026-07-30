#ifndef AGENTIC_HARNESS_H
#define AGENTIC_HARNESS_H

#include <string>
#include <vector>

#include "ModelContext.hpp"
#include "ModelRequest.hpp"
#include "ModelSystemPrompt.hpp"
#include "../util/Handle.hpp"
#include "../util/RefCounted.hpp"

class Model;
class ModelToolBindings;

/**
 * Defines a collection of models, tools and  system prompts that can be used for various operations, depending on their
 * capabilities.
 */
class AgenticHarness : public RefCounted
{
	public:

		/**
		 * Role that an assigned Agent Model plays within the collection.
		 */
		enum class Role
		{
			/// Used for general chat interface.
			CHAT,
			/// Drives hive level agentic interaction, i.e. planning or querying the overall structure of the hive.
			HIVE,
			/// Drives node level agentic interaction, i.e. querying, setting up or altering a nodes state.
			NODE,
			/// Drives script level agentic interaction, i.e. querying or building a script for a particular purpose.
			SCRIPT
		};

		/**
		 * Broad agent model capabilities.
		 */
		enum class Capability
		{
			/// Can do simple tasks well.
			LOW,
			/// Can do average tasks well.
			MEDIUM,
			/// Can do complex tasks well.
			HIGH
		};

		/**
		 * Pairs a role with the capability required of a model fulfilling it.
		 */
		struct RoleCapability
		{
			/// Role the capability applies to.
			Role role;

			/// Capability required of a model fulfilling the role.
			Capability capability;
		};

		/**
		 * Pairs a model with a role and capability.
		 */
		struct ModelAssignment
		{
			/// Role/capability the model is assigned to.
			RoleCapability roleCapability;

			/// Model that is to fulfill the role with capability.
			Handle<Model> model;

			/// Sampling temperature every request serviced by the model in this role is made at, or
			/// ModelRequest::PROVIDER_DEFAULT_TEMPERATURE where the choice is left to the provider.
			double temperature;
		};

		/**
		 * Pairs a system prompt with the role/capability it is assigned to.
		 */
		struct SystemPromptAssignment
		{
			/// Role/capability the system prompt applies to.
			RoleCapability roleCapability;

			/// System prompt that is to be used for the role with capability.
			ModelSystemPrompt systemPrompt;
		};

		/**
		 * Pairs a tool binding with the role/capability combinations that may use it.
		 */
		struct ToolBindingAssignment
		{
			/// Role/capability combinations that may use the tool.
			std::vector<RoleCapability> roleCapabilities;

			/// Tool binding that is made available.
			Handle<ModelToolBindings> tool;
		};

		AgenticHarness();

		/**
		 * Assign a model to a role and capability.
		 * @param roleCapability Role and capability to assign the model to.
		 * @param model Model to assign.
		 * @param temperature Sampling temperature every request serviced by the model in this role is to
		 *        be made at, from 0 up to ModelRequest::MAX_TEMPERATURE. Left at
		 *        ModelRequest::PROVIDER_DEFAULT_TEMPERATURE, no temperature is asked for and the provider
		 *        answers at whatever it defaults to.
		 * @throw AgentException When the temperature is neither ModelRequest::PROVIDER_DEFAULT_TEMPERATURE
		 *        nor within the permitted range. Nothing is assigned when it is refused.
		 * @note A temperature is cut to MODEL_TEMPERATURE_DECIMAL_PLACES decimal places on its way to a
		 *       provider, so asking for more precision than that gains nothing.
		 */
		void addModelAssignment(RoleCapability roleCapability, Handle<Model> model,
			double temperature = ModelRequest::PROVIDER_DEFAULT_TEMPERATURE);

		/**
		 * Get all role/model assignments, in the order they were added.
		 */
		std::vector<ModelAssignment> getModelAssignments();

		/**
		 * Assign a system prompt to a role and capability.
		 * @param roleCapability Role and capability to assign the system prompt to.
		 * @param systemPrompt System prompt to assign.
		 */
		void addSystemPrompt(RoleCapability roleCapability, ModelSystemPrompt systemPrompt);

		/**
		 * Get all role/system prompt assignments, in the order they were added.
		 */
		std::vector<SystemPromptAssignment> getSystemPrompts();

		/**
		 * Add a tool binding, restricted to the given role/capability combinations.
		 * @param roleCapabilities Role/capability combinations that may use the tool.
		 * @param tool Tool bindings to add.
		 */
		void addToolBinding(std::vector<RoleCapability> roleCapabilities, Handle<ModelToolBindings> tools);

		/**
		 * Get all tool bindings, in the order they were added.
		 */
		std::vector<ToolBindingAssignment> getToolBindings();

		/**
		 * Create a new model context for the given role and capability, without processing any prompt
		 * against it.
		 * @param role Role the context is being created for.
		 * @param capability Capability required of the model that will eventually service requests made in
		 *        the context.
		 * @returns The new context, built from the system prompts and tools assigned to the role and
		 *          capability.
		 */
		Handle<ModelContext> createContext(Role role, Capability capability);

		/**
		 * Process a request against whichever assigned model matches the given role and capability.
		 * @param prompt Text of the prompt to send to the model.
		 * @param role Role the request is being made for. Matched exactly against assigned models.
		 * @param capability Capability required of the model. Matched exactly against assigned models.
		 * @param context Context of a previous interaction to continue, carrying the system prompts, tools
		 *        and chat history it was built with. When not supplied, a new context is built from the
		 *        system prompts and tools assigned to the role and capability.
		 * @returns The context the request was serviced within, with the prompt just processed, any tool
		 *          calls made while working on it and the response given to it appended to its chat
		 *          history. Pass it back in to continue the same conversation.
		 * @throw AgentException When no prompt text was supplied, or when no candidate model is assigned to
		 *        the role with that capability.
		 */
		Handle<ModelContext> processRequest(std::string prompt, Role role, Capability capability,
			Handle<ModelContext> context = Handle<ModelContext>(0));

	protected:

		// Required by ref counting.
		virtual ~AgenticHarness();

	private:

		// Disable copying.
		AgenticHarness(const AgenticHarness& copyFrom);
		AgenticHarness& operator= (const AgenticHarness& copyFrom);

		/// Role/model assignments, in the order they were added.
		std::vector<ModelAssignment> _assignments;

		/// Role/system prompt assignments, in the order they were added.
		std::vector<SystemPromptAssignment> _systemPrompts;

		/// Tool bindings, in the order they were added.
		std::vector<ToolBindingAssignment> _toolBindings;
};

#endif
