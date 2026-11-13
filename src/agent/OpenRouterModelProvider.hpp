#ifndef OPEN_ROUTER_MODEL_PROVIDER_H
#define OPEN_ROUTER_MODEL_PROVIDER_H

#include <string>
#include <vector>

#include "AgentException.hpp"
#include "ModelProvider.hpp"
#include "../rapidjson/fwd.h"
#include "../util/Handle.hpp"

class Model;
class ModelRequest;

/**
 * Model provider for OpenRouter, which serves models from many vendors behind one OpenAI style API.
 */
class OpenRouterModelProvider : public ModelProvider
{
	public:

		/// URL of OpenRouter's API, used when a harness does not name one.
		static constexpr const char* DEFAULT_URL = "https://openrouter.ai/api/v1";

		/**
		 * Connect to OpenRouter.
		 * @param url URL of the API, including scheme and version path, e.g. DEFAULT_URL.
		 * @param apiKey Key that requests are authenticated with.
		 * @throw AgentException When the API cannot be reached, rejects the key, or returns a model list that
		 *        cannot be read.
		 */
		OpenRouterModelProvider(std::string url, std::string apiKey);

	protected:

		virtual ~OpenRouterModelProvider();

		virtual void _populateModels() override;

		virtual std::string _processRequest(Handle<Model> model, ModelRequest& request,
			std::vector<ModelContext::ToolCallRound>& toolCallRounds) override;

		virtual std::string _requestHeaders() override;

	private:

		// Disable copying.
		OpenRouterModelProvider(const OpenRouterModelProvider& copyFrom);
		OpenRouterModelProvider& operator= (const OpenRouterModelProvider& copyFrom);

		/**
		 * Verify that the API accepts the key this provider was given.
		 * @throw AgentException When it does not.
		 */
		void __checkApiKey();

		/**
		 * Raise the exception for an error OpenRouter reported, carrying its explanation as the detail.
		 * @param errorValue The "error" member of a response, either at its top level or on a choice.
		 * @param fallback Error raised when the error's code does not name a more specific one.
		 * @throw AgentException Always: AUTHENTICATION_FAILED for code 401, INSUFFICIENT_CREDITS for 402,
		 *        MODEL_RATE_LIMITED for 429, and fallback otherwise.
		 * @note OpenRouter answers an error with its HTTP status as the "code", a general "message", and,
		 *       when the fault lay with the host serving the model rather than with OpenRouter, that host's
		 *       own explanation in "metadata.raw" alongside its name in "metadata.provider_name".
		 */
		[[noreturn]] static void __throwProviderError(const rapidjson::Value& errorValue,
			AgentException::Error fallback);

		/// URL of the API, including scheme and version path.
		std::string _url;

		/// Key that requests are authenticated with.
		std::string _apiKey;
};

#endif
