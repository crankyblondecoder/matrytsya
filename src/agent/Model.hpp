#ifndef MODEL_H
#define MODEL_H

#include <string>

#include "../util/RefCounted.hpp"
#include "../util/Handle.hpp"

class ModelProvider;
class ModelRequest;

/**
 * Describes an AI model that can be used by the agentic harness of a graph hive.
 */
class Model : public RefCounted
{
	public:

		/// When true, every prompt sent to a model and every response received back from it are written
		/// to standard output.
		static bool _logToConsole;

		/**
		 * Get the provider that this model was sourced from.
		 */
		Handle<ModelProvider> getProvider();

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

		/**
		 * Send a request to this model.
		 * @param request Request to send to the model.
		 * @returns The result of the request.
		 */
		virtual std::string processRequest(ModelRequest& request) = 0;

	protected:

		virtual ~Model();

		/**
		 * Create a description of an AI model.
		 * @param provider Provider that this model was sourced from.
		 * @param name Name of the model.
		 * @param description Description of the model.
		 * @param inputTokenCost Cost of an input token.
		 * @param outputTokenCost Cost of an output token.
		 * @param free Whether the model is free to use.
		 */
		Model(Handle<ModelProvider> provider, std::string name, std::string description, std::string inputTokenCost,
			std::string outputTokenCost, bool free);

		/**
		 * Standard processing of a model request.
		 * @param request Request to send to the model.
		 * @returns The result of the request.
		 */
		std::string _processRequest(ModelRequest& request);

	private:

		// Disable copying.
		Model(const Model& copyFrom);
		Model& operator= (const Model& copyFrom);

		/// The provider that this model was sourced from.
		Handle<ModelProvider> _provider;

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
