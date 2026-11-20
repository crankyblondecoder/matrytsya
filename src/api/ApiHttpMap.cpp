#include "ApiHttpMap.hpp"

#include "ApiAuthenticator.hpp"
#include "ApiController.hpp"
#include "ApiException.hpp"
#include "../display/http/HttpRequest.hpp"
#include "../display/http/HttpResponse.hpp"
#include "../display/http/HttpServerBase.hpp"
#include "../graph/GraphException.hpp"
#include "../log/log.hpp"
#include "../persist/PersistException.hpp"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

#include <ctype.h>
#include <exception>

namespace
{
	/// Base path of the API. Every request below the mount path must start with it.
	const char* const _BASE_PATH = "/api/v1";

	std::string lowerCase(std::string value)
	{
		for(char& character : value) character = (char) tolower((unsigned char) character);

		return value;
	}

	// Whether a persistence error is about what the client sent, rather than about the server's own storage.
	bool isClientPersistError(PersistException::Error error)
	{
		switch(error)
		{
			case PersistException::LMDB_OPEN_FAILED:
			case PersistException::LMDB_TXN_FAILED:
			case PersistException::LMDB_CORRUPT_RECORD:
			case PersistException::LMDB_UNSUPPORTED_FORMAT:
			case PersistException::HIVE_NOT_CAPTURABLE:
				return false;

			default:
				return true;
		}
	}
}

ApiHttpMap::ApiHttpMap(HttpServerBase& httpServer, ApiAuthenticator& authenticator, ApiController& controller,
	std::string path) :
	_httpServer{httpServer}, _authenticator{authenticator}, _controller{controller}, _path{path}
{
	_httpServer.addHandler(_path, this);
}

ApiHttpMap::~ApiHttpMap()
{
	_httpServer.removeHandler(_path);
}

bool ApiHttpMap::retainHandler()
{
	return incrRef();
}

void ApiHttpMap::releaseHandler()
{
	decrRef();
}

void ApiHttpMap::handleRequest(HttpRequest& request, HttpResponse& response)
{
	// Nothing the API answers may be cached, framed or read as anything but what it says it is.
	response.addHeader("Cache-Control", "no-store");
	response.addHeader("X-Content-Type-Options", "nosniff");
	response.addHeader("Content-Security-Policy", "default-src 'none'; frame-ancestors 'none'");

	ApiAuthenticator::Result authentication = _authenticator.authenticate(request);

	if(authentication == ApiAuthenticator::RATE_LIMITED)
	{
		response.addHeader("Retry-After", std::to_string(ApiAuthenticator::FAILURE_WINDOW_SECONDS));

		__respondError(response, 429, ApiException::getErrorCode(ApiException::RATE_LIMITED),
			"Too many requests from this address failed to authenticate. Wait before trying again.");
		return;
	}

	if(authentication != ApiAuthenticator::ACCEPTED)
	{
		response.addHeader("WWW-Authenticate", "Bearer realm=\"matrytsya\"");

		__respondError(response, 401, ApiException::getErrorCode(ApiException::UNAUTHORISED),
			"A valid API token is required, as \"Authorization: Bearer <token>\".");
		return;
	}

	std::string path = request.getPath();
	std::string basePath = _BASE_PATH;

	if(path != basePath && path.compare(0, basePath.size() + 1, basePath + "/") != 0)
	{
		__respondError(response, 404, ApiException::getErrorCode(ApiException::NOT_FOUND),
			"The API is served below " + basePath + "/.");
		return;
	}

	if(request.getBody().find_first_not_of(" \t\r\n") != std::string::npos &&
		lowerCase(request.getHeader("Content-Type")).compare(0, 16, "application/json") != 0)
	{
		__respondError(response, 415, ApiException::getErrorCode(ApiException::UNSUPPORTED_MEDIA_TYPE),
			"A request body must be sent as \"Content-Type: application/json\".");
		return;
	}

	std::string routePath = path.substr(basePath.size());

	if(routePath.empty()) routePath = "/";

	try
	{
		_controller.dispatch(request, routePath, response);
	}
	catch(ApiException& exception)
	{
		ApiException::Error error = exception.getError();

		__respondError(response, ApiException::getHttpStatus(error), ApiException::getErrorCode(error),
			exception.getDetail().empty() ? ApiException::getErrorCode(error) : exception.getDetail());
	}
	catch(PersistException& exception)
	{
		if(isClientPersistError(exception.getError()))
		{
			std::string message = PersistException::getErrorDescription(exception.getError());

			if(!exception.getDetail().empty()) message += " -- " + exception.getDetail();

			__respondError(response, 422, ApiException::getErrorCode(ApiException::UNPROCESSABLE), message);
		}
		else
		{
			LOG(Logger::ERROR, std::string("Control API request failed: ") +
				PersistException::getErrorDescription(exception.getError()) + " -- " + exception.getDetail())

			__respondError(response, 500, "INTERNAL", "The server could not carry out the request.");
		}
	}
	catch(GraphException& exception)
	{
		if(exception.getError() == GraphException::DUPLICATE_HIVE)
		{
			__respondError(response, 409, ApiException::getErrorCode(ApiException::CONFLICT),
				"A hive of that name is already running.");
		}
		else
		{
			LOG(Logger::ERROR, "Control API request failed with graph error " + std::to_string(exception.getError()))

			__respondError(response, 500, "INTERNAL", "The server could not carry out the request.");
		}
	}
	catch(Exception& exception)
	{
		LOG(Logger::ERROR, "Control API request failed with an error from module " +
			std::to_string(exception.getModule()))

		__respondError(response, 500, "INTERNAL", "The server could not carry out the request.");
	}
	catch(std::exception& exception)
	{
		LOG(Logger::ERROR, std::string("Control API request failed: ") + exception.what())

		__respondError(response, 500, "INTERNAL", "The server could not carry out the request.");
	}
	catch(...)
	{
		LOG(Logger::ERROR, "Control API request failed")

		__respondError(response, 500, "INTERNAL", "The server could not carry out the request.");
	}
}

void ApiHttpMap::__respondError(HttpResponse& response, unsigned status, const std::string& code,
	const std::string& message)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

	writer.StartObject();
	writer.Key("error");
	writer.StartObject();
	writer.Key("code");
	writer.String(code.c_str(), (rapidjson::SizeType) code.size());
	writer.Key("message");
	writer.String(message.c_str(), (rapidjson::SizeType) message.size());
	writer.EndObject();
	writer.EndObject();

	response.setStatus(status);
	response.setContentType("application/json");
	response.setBody(std::string(buffer.GetString(), buffer.GetSize()));
}
