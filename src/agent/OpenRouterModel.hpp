#ifndef OPEN_ROUTER_MODEL_H
#define OPEN_ROUTER_MODEL_H

#include <string>

#include "Model.hpp"
#include "../util/Handle.hpp"

class ModelProvider;
class ModelRequest;

/**
 * AI model hosted by OpenRouter.
 */
class OpenRouterModel : public Model
{
	public:

		/**
		 * Create a description of an OpenRouter hosted AI model.
		 * @param provider Provider that this model was sourced from.
		 * @param name Name of the model.
		 * @param description Description of the model.
		 * @param inputTokenCost Cost of an input token.
		 * @param outputTokenCost Cost of an output token.
		 * @param free Whether the model is free to use.
		 */
		OpenRouterModel(Handle<ModelProvider> provider, std::string name, std::string description,
			std::string inputTokenCost, std::string outputTokenCost, bool free);

		virtual std::string processRequest(ModelRequest& request) override;

	protected:

		virtual ~OpenRouterModel(){}

	private:

		// Disable copying.
		OpenRouterModel(const OpenRouterModel& copyFrom);
		OpenRouterModel& operator= (const OpenRouterModel& copyFrom);
};

#endif
