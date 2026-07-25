#ifndef AGENTIC_HARNESS_H
#define AGENTIC_HARNESS_H

#include <string>
#include <vector>

#include "ModelContext.hpp"
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
			/// Drives hive level decisions, i.e. The overall structure of the hive.
			HIVE,
			/// Drives node level decisions, i.e. Node state to drive a particular behaviour, including
			/// agentic requests inside scripts.
			NODE,
			/// Drives script level decisions, i.e. building a script for a particular purpose.
			SCRIPT
		};

		/**
		 * Broad agent model capabilities.
		 * @note A more capable agent can be substituted for a less capable one.
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
		 */
		void addModelAssignment(RoleCapability roleCapability, Handle<Model> model);

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
		 * Process a request against whichever assigned model is best suited to the given role and capability.
		 * @param prompt Text of the prompt to send to the model.
		 * @param role Role the request is being made for. Matched exactly against assigned models.
		 * @param capability Capability required of the model. A model assigned a higher capability than
		 *        requested may be substituted, per the note on Capability.
		 * @param context Context of a previous interaction to continue, carrying the system prompts, tools
		 *        and chat history it was built with. When not supplied, a new context is built from the
		 *        system prompts and tools assigned to the role and capability.
		 * @returns The context the request was serviced within, with the prompt just processed, any tool
		 *          calls made while working on it and the response given to it appended to its chat
		 *          history. Pass it back in to continue the same conversation.
		 * @throw AgentException When no prompt text was supplied, or when no candidate model is assigned to
		 *        the role with sufficient capability.
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
