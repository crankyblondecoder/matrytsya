#ifndef AGENT_MODEL_H
#define AGENT_MODEL_H

#include <string>

#include "../util/Handle.hpp"

class AgentModelProvider;

/**
 * Describes an AI model that can be used by the agentic harness of a graph hive.
 */
class AgentModel
{
	public:

		/**
		 * Create a description of an AI model.
		 * @param provider Provider that this model was sourced from.
		 * @param name Name of the model.
		 * @param description Description of the model.
		 * @param inputTokenCost Cost of an input token.
		 * @param outputTokenCost Cost of an output token.
		 * @param free Whether the model is free to use.
		 */
		AgentModel(Handle<AgentModelProvider> provider, std::string name, std::string description,
			std::string inputTokenCost, std::string outputTokenCost, bool free);

		/**
		 * Get the provider that this model was sourced from.
		 */
		Handle<AgentModelProvider> getProvider();

		/**
		 * Get the name of this model.
		 */
		std::string getName();

		/**
		 * Get the description of this model.
		 */
		std::string getDescription();

		/**
		 * Get the cost of an input token for this model.
		 */
		std::string getInputTokenCost();

		/**
		 * Get the cost of an output token for this model.
		 */
		std::string getOutputTokenCost();

		/**
		 * Whether this model is free to use.
		 */
		bool getFree();

	protected:

	private:

		/// The provider that this model was sourced from.
		Handle<AgentModelProvider> _provider;

		/// The name of the model.
		std::string _name;

		/// The description of the model.
		std::string _description;

		/// The cost of an input token.
		std::string _inputTokenCost;

		/// The cost of an output token.
		std::string _outputTokenCost;

		/// Whether the model is free to use.
		bool _free;
};

#endif
