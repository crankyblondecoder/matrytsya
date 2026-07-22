#ifndef AGENT_MODEL_PROVIDER_H
#define AGENT_MODEL_PROVIDER_H

#include <string>
#include <vector>

#include "../util/RefCounted.hpp"
#include "AgentModel.hpp"

struct mg_connection;

/**
 * Base class of all classes that provide AI models.
 * Subclasses communicate with their provider over HTTP.
 * A concrete provider should know how to connect to its server and which endpoints to hit.
 */
class AgentModelProvider : public RefCounted
{
	public:

		/**
		 * Get the models only available from this provider.
		 */
		std::vector<AgentModel> getModels();

		/**
		 * Clear all currently available models and fetch and re-populate them.
		 */
		void refreshModels();

	protected:

		virtual ~AgentModelProvider();

		// It makes no sense for this to be instantiable by itself.
		AgentModelProvider();

		/**
		 * Fetch and populate all models available from this provider.
		 */
		virtual void _populateModels() = 0;

		/**
		 * Add a model only available from this provider.
		 * @param model Model to add.
		 */
		void _addModel(AgentModel model);

		/**
		 * Verify that a server can be reached.
		 * @param url URL of the server, including port.
		 * @throw AgentException When the server cannot be reached.
		 */
		void _checkConnection(std::string url);

		/**
		 * Perform a blocking HTTP GET request.
		 * @param url Full URL to request.
		 * @returns Response body.
		 * @throw AgentException When the request fails or times out.
		 */
		std::string _httpGet(std::string url);

	private:

		// Disable copying.
		AgentModelProvider(const AgentModelProvider& copyFrom);
		AgentModelProvider& operator= (const AgentModelProvider& copyFrom);

		/**
		 * Mongoose event handler callback used while checking a connection.
		 * @param connection Connection the event occurred on.
		 * @param event Event type.
		 * @param eventData Event specific data.
		 */
		static void __checkConnectionEventHandler(mg_connection* connection, int event, void* eventData);

		/**
		 * Mongoose event handler callback used while performing an HTTP GET request.
		 * @param connection Connection the event occurred on.
		 * @param event Event type.
		 * @param eventData Event specific data.
		 */
		static void __httpGetEventHandler(mg_connection* connection, int event, void* eventData);

		/// Models only available from this provider.
		std::vector<AgentModel> _models;
};

#endif
