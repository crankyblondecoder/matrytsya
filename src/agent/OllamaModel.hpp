#ifndef OLLAMA_MODEL_H
#define OLLAMA_MODEL_H

#include <string>

#include "Model.hpp"
#include "../util/Handle.hpp"

class ModelProvider;
class ModelRequest;

/**
 * AI model hosted by an Ollama server.
 */
class OllamaModel : public Model
{
	public:

		/**
		 * Create a description of an Ollama hosted AI model.
		 * @param provider Provider that this model was sourced from.
		 * @param name Name of the model.
		 * @param description Description of the model.
		 * @param inputTokenCost Cost of an input token.
		 * @param outputTokenCost Cost of an output token.
		 * @param free Whether the model is free to use.
		 */
		OllamaModel(Handle<ModelProvider> provider, std::string name, std::string description,
			std::string inputTokenCost, std::string outputTokenCost, bool free);

		virtual std::string processRequest(ModelRequest& request) override;

	protected:

		virtual ~OllamaModel(){}

	private:

		// Disable copying.
		OllamaModel(const OllamaModel& copyFrom);
		OllamaModel& operator= (const OllamaModel& copyFrom);
};

#endif
