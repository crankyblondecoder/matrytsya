#ifndef API_EXCEPTION_H
#define API_EXCEPTION_H

#include <string>

#include "../util/Exception.hpp"

/**
 * Exception raised by the api module.
 * @note Each error corresponds to one error code a client of the control API is given, see
 *       ApiException::getErrorCode().
 */
class ApiException : public Exception
{
	public:

		enum Error
		{
			UNKNOWN,
			/// The request carried no valid API token.
			UNAUTHORISED,
			/// Too many requests from the client's address failed authentication recently.
			RATE_LIMITED,
			/// No resource exists at the requested path.
			NOT_FOUND,
			/// The resource exists but does not accept the request's method.
			METHOD_NOT_ALLOWED,
			/// The request's body is not JSON, or is malformed in a way that is not about what it describes.
			BAD_REQUEST,
			/// The request's body was not declared as JSON.
			UNSUPPORTED_MEDIA_TYPE,
			/// The request conflicts with the current state of the hive, e.g. a name already in use.
			CONFLICT,
			/// The request is well formed but what it asks for cannot be done, e.g. an invalid node definition.
			UNPROCESSABLE,
			/// The server was asked to accept API requests in a way that would expose the API token.
			INSECURE_CONFIGURATION,
			/// No API token, or one too short to be safe, was supplied to the server.
			MISSING_API_TOKEN
		};

		virtual ~ApiException(){}

		ApiException(Error error, std::string detail = "") :
			Exception(Exception::API), _error{error}, _detail{detail} {}

		Error getError() {return _error;}

		/**
		 * Get the detail of this exception, which says what in particular was wrong.
		 * @returns Detail text, safe to pass on to a client. Empty if there is none.
		 */
		std::string getDetail() {return _detail;}

		/**
		 * Get the error code a client is given for an error, e.g. "NOT_FOUND".
		 */
		static const char* getErrorCode(Error error)
		{
			switch(error)
			{
				case UNAUTHORISED: return "UNAUTHORISED";
				case RATE_LIMITED: return "RATE_LIMITED";
				case NOT_FOUND: return "NOT_FOUND";
				case METHOD_NOT_ALLOWED: return "METHOD_NOT_ALLOWED";
				case BAD_REQUEST: return "BAD_REQUEST";
				case UNSUPPORTED_MEDIA_TYPE: return "UNSUPPORTED_MEDIA_TYPE";
				case CONFLICT: return "CONFLICT";
				case UNPROCESSABLE: return "UNPROCESSABLE";
				case INSECURE_CONFIGURATION: return "INSECURE_CONFIGURATION";
				case MISSING_API_TOKEN: return "MISSING_API_TOKEN";

				default: return "INTERNAL";
			}
		}

		/**
		 * Get the HTTP status code a client is given for an error.
		 */
		static unsigned getHttpStatus(Error error)
		{
			switch(error)
			{
				case UNAUTHORISED: return 401;
				case RATE_LIMITED: return 429;
				case NOT_FOUND: return 404;
				case METHOD_NOT_ALLOWED: return 405;
				case BAD_REQUEST: return 400;
				case UNSUPPORTED_MEDIA_TYPE: return 415;
				case CONFLICT: return 409;
				case UNPROCESSABLE: return 422;

				default: return 500;
			}
		}

	private:

		Error _error;

		/// What in particular was wrong.
		std::string _detail;
};

#endif
