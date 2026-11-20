#ifndef API_HTTP_MAP_H
#define API_HTTP_MAP_H

#include <string>

#include "../display/http/HttpRequestHandler.hpp"
#include "../util/RefCounted.hpp"

class ApiAuthenticator;
class ApiController;
class HttpServerBase;

/**
 * Serves the hive control API over HTTP.
 *
 * Every request must authenticate with an API token. A request with a body must declare it as JSON. Whatever the
 * outcome, the response is JSON: an error is given as {"error": {"code": ..., "message": ...}}, where the code is
 * one of the ApiException error codes.
 *
 * @note Mounts itself on the server when constructed and unmounts itself when destroyed.
 */
class ApiHttpMap : public RefCounted, public HttpRequestHandler
{
	public:

		/**
		 * @param httpServer Server to mount on. Must outlive this.
		 * @param authenticator Authenticates every request. Must outlive this.
		 * @param controller Carries out every authenticated request. Must outlive this.
		 * @param path Path to mount at. Every request below it whose path starts with the API's base path is served.
		 * @throw DisplayException(DUPLICATE_HTTP_HANDLER_PATH) If something is already mounted at the path.
		 */
		ApiHttpMap(HttpServerBase& httpServer, ApiAuthenticator& authenticator, ApiController& controller,
			std::string path = "/api");

		void handleRequest(HttpRequest& request, HttpResponse& response) final;

		bool retainHandler() final;

		void releaseHandler() final;

	protected:

		// Required by ref counting.
		virtual ~ApiHttpMap();

	private:

		// Disable copying.
		ApiHttpMap(const ApiHttpMap& copyFrom);
		ApiHttpMap& operator= (const ApiHttpMap& copyFrom);

		/**
		 * Populate a response with an error.
		 * @param response Response to populate.
		 * @param status HTTP status code.
		 * @param code Error code, e.g. "NOT_FOUND".
		 * @param message Message saying what went wrong, for a person or agent to act on.
		 */
		static void __respondError(HttpResponse& response, unsigned status, const std::string& code,
			const std::string& message);

		/// Server this is mounted on.
		HttpServerBase& _httpServer;

		/// Authenticates every request.
		ApiAuthenticator& _authenticator;

		/// Carries out every authenticated request.
		ApiController& _controller;

		/// Path this is mounted at.
		std::string _path;
};

#endif
