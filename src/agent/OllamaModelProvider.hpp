#ifndef OLLAMA_MODEL_PROVIDER_H
#define OLLAMA_MODEL_PROVIDER_H

#include <string>
#include <vector>

#include "ModelProvider.hpp"
#include "../util/Handle.hpp"

class Model;
class ModelRequest;

/**
 * Model provider for a locally or remotely hosted Ollama server.
 */
class OllamaModelProvider : public ModelProvider
{
	public:

		/**
		 * Connect to an Ollama server.
		 * @param url URL of the Ollama server, including port.
		 * @throw AgentException When the server cannot be reached.
		 */
		OllamaModelProvider(std::string url);

	protected:

		virtual ~OllamaModelProvider();

		void _populateModels() override;

		std::string _processRequest(Handle<Model> model, ModelRequest& request,
			std::vector<ModelContext::ToolCallRound>& toolCallRounds) override;

	private:

		// Disable copying.
		OllamaModelProvider(const OllamaModelProvider& copyFrom);
		OllamaModelProvider& operator= (const OllamaModelProvider& copyFrom);

		/// URL of the Ollama server, including port.
		std::string _url;
};

#endif
