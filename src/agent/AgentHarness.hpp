#ifndef AGENT_HARNESS_H
#define AGENT_HARNESS_H

#include <vector>

#include "AgentModel.hpp"

/**
 * Defines an agentic harness so that multiple models can be used for various operations, depending on their
 * capabilities.
 */
class AgentHarness
{
	public:

		/**
		 * Role that an assigned Agent Model plays within the harness.
		 */
		enum class Role
		{
			/// Used as a fallback.
			GENERAL,
			/// Drives hive level decisions.
			HIVE,
			/// Drives node level decisions.
			NODE,
			/// Drives script level decisions.
			SCRIPT
		};

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
		 * Pairs a model with a role and capability.
		 */
		struct ModelAssignment
		{
			/// Role the model is assigned to.
			Role role;

			/// Capability the model can provide in its role.
			Capability capability;

			/// Model that is to fulfill the role with capability.
			AgentModel model;
		};

		AgentHarness();

		virtual ~AgentHarness();

		/**
		 * Assign a model to a role and capability.
		 * @param role Role to assign the model to.
		 * @param capability Capability the model provides for the role.
		 * @param model Model to assign.
		 */
		void addModelAssignment(Role role, Capability capability, AgentModel model);

		/**
		 * Get all role/model assignments, in the order they were added.
		 */
		std::vector<ModelAssignment> getModelAssignments();

	protected:

	private:

		// Disable copying.
		AgentHarness(const AgentHarness& copyFrom);
		AgentHarness& operator= (const AgentHarness& copyFrom);

		/// Role/model assignments, in the order they were added.
		std::vector<ModelAssignment> _assignments;
};

#endif
