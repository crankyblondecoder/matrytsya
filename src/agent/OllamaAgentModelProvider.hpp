#ifndef OLLAMA_AGENT_MODEL_PROVIDER_H
#define OLLAMA_AGENT_MODEL_PROVIDER_H

#include <string>

#include "AgentModelProvider.hpp"

/**
 * Model provider for a locally or remotely hosted Ollama server.
 */
class OllamaAgentModelProvider : public AgentModelProvider
{
	public:

		/**
		 * Connect to an Ollama server.
		 * @param url URL of the Ollama server, including port.
		 * @throw AgentException When the server cannot be reached.
		 */
		OllamaAgentModelProvider(std::string url);

	protected:

		virtual ~OllamaAgentModelProvider();

		void _populateModels() override;

	private:

		// Disable copying.
		OllamaAgentModelProvider(const OllamaAgentModelProvider& copyFrom);
		OllamaAgentModelProvider& operator= (const OllamaAgentModelProvider& copyFrom);

		/// URL of the Ollama server, including port.
		std::string _url;
};

#endif
