#ifndef MODEL_PROVIDER_H
#define MODEL_PROVIDER_H

#include <string>
#include <vector>

#include "ModelContext.hpp"
#include "../util/RefCounted.hpp"
#include "../util/Handle.hpp"

class Model;
class ModelRequest;
struct mg_connection;

/**
 * Base class of all classes that provide AI models.
 * Subclasses communicate with their provider over HTTP.
 * A concrete provider should know how to connect to its server and which endpoints to hit.
 */
class ModelProvider : public RefCounted
{
	public:

		/**
		 * Get the models only available from this provider.
		 */
		std::vector<Handle<Model>> getModels();

		/**
		 * Clear all currently available models and fetch and re-populate them.
		 */
		void refreshModels();

		/**
		 * Send a request to a model sourced from this provider.
		 * @param model Model the request was originally made on.
		 * @param request Request to send to the model.
		 * @returns The result of the request.
		 * @note The prompt, the tool calls the model made and the result are appended to the chat history
		 *       of the request's context, so a context reused for a further request carries the
		 *       conversation so far. Nothing is recorded when the request fails.
		 * @throw AgentException When a request is already being serviced in the request's context, as a
		 *        context carries one conversation and cannot hold two at once.
		 */
		std::string processRequest(Handle<Model> model, ModelRequest& request);

	protected:

		/// Rounds of tool calls a model may request while servicing one request before it is abandoned.
		static constexpr unsigned MAX_TOOL_CALL_ROUNDS = 16;

		/// Characters of a provider's own explanation kept in an error's detail, which can otherwise run to a
		/// whole upstream response.
		static constexpr size_t MAX_ERROR_DETAIL_LENGTH = 500;

		/**
		 * Cut text meant for an error's detail down to MAX_ERROR_DETAIL_LENGTH characters.
		 * @param text Text to cut.
		 * @returns The text, marked where it was cut if it was too long.
		 */
		static std::string _truncateDetail(const std::string& text);

		/**
		 * Describe a response that could not be read, for an error's detail.
		 * @param body Body of the response as it was received.
		 * @returns The description.
		 * @note What a body that could not be read begins with usually says where it came from, e.g. the HTML
		 *       error page of a proxy in front of the provider.
		 */
		static std::string _unreadableResponseDetail(const std::string& body);

		virtual ~ModelProvider();

		// It makes no sense for this to be instantiable by itself.
		ModelProvider();

		/**
		 * Fetch and populate all models available from this provider.
		 */
		virtual void _populateModels() = 0;

		/**
		 * Send a request to a model sourced from this provider.
		 * @param model Model the request was originally made on.
		 * @param request Request to send to the model.
		 * @param toolCallRounds Rounds of tool calls the model asks for while servicing the request, to be
		 *        appended to in the order it asks for them, so that they can be recorded in the context.
		 * @returns The result of the request.
		 */
		virtual std::string _processRequest(Handle<Model> model, ModelRequest& request,
			std::vector<ModelContext::ToolCallRound>& toolCallRounds) = 0;

		/**
		 * Add a model only available from this provider.
		 * @param model Model to add.
		 */
		void _addModel(Handle<Model> model);

		/**
		 * Verify that a server can be reached.
		 * @param url URL of the server, including port.
		 * @throw AgentException When the server cannot be reached, or when the thread this is called on is
		 *        asked to stop before it answers.
		 */
		void _checkConnection(std::string url);

		/**
		 * Perform a blocking HTTP GET request.
		 * @param url Full URL to request.
		 * @returns Response body.
		 * @throw AgentException When the request fails or times out, or when the thread this is called on is
		 *        asked to stop before the response arrives.
		 */
		std::string _httpGet(std::string url);

		/**
		 * Perform a blocking HTTP POST request with a JSON body.
		 * @param url Full URL to request.
		 * @param body Request body to send.
		 * @returns Response body.
		 * @note This allows far longer than a plain GET, as a provider servicing a request has to run
		 *       inference before it can reply.
		 * @note The wait is abandoned as soon as the thread this is called on is asked to stop, so that a
		 *       request in flight cannot hold a worker up for the whole of a shutdown.
		 * @throw AgentException When the request fails or times out, or when the thread this is called on is
		 *        asked to stop before the response arrives.
		 */
		std::string _httpPost(std::string url, std::string body);

		/**
		 * Get the headers to send with every HTTP request made to this provider, beyond those every request
		 * carries anyway, e.g. to authenticate it.
		 * @returns Complete header lines, each terminated by "\r\n". Empty when there are none.
		 */
		virtual std::string _requestHeaders();

	private:

		// Disable copying.
		ModelProvider(const ModelProvider& copyFrom);
		ModelProvider& operator= (const ModelProvider& copyFrom);

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

		/**
		 * Mongoose event handler callback used while performing an HTTP POST request.
		 * @param connection Connection the event occurred on.
		 * @param event Event type.
		 * @param eventData Event specific data.
		 */
		static void __httpPostEventHandler(mg_connection* connection, int event, void* eventData);

		/// Models only available from this provider.
		std::vector<Handle<Model>> _models;
};

#endif
