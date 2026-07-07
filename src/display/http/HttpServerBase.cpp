#include "HttpServerBase.hpp"

#include "../../thread/thread.hpp"
#include "../DisplayException.hpp"
#include "HttpRequest.hpp"
#include "HttpRequestHandler.hpp"
#include "HttpResponse.hpp"

HttpServerBase::HttpServerBase(unsigned port) : _port{port}
{
}

HttpServerBase::~HttpServerBase()
{
}

void HttpServerBase::start()
{
	_start();
}

void HttpServerBase::stop()
{
	_stop();
}

void HttpServerBase::addHandler(std::string path, HttpRequestHandler* handler)
{
	{ SYNC(_lock)

		if(_handlers.find(path) != _handlers.end())
		{
			throw DisplayException(DisplayException::DUPLICATE_HTTP_HANDLER_PATH);
		}

		_handlers[path] = handler;
	}
}

void HttpServerBase::removeHandler(std::string path)
{
	{ SYNC(_lock)

		_handlers.erase(path);
	}
}

unsigned HttpServerBase::getPort()
{
	return _port;
}

void HttpServerBase::_routeRequest(HttpRequest& request, HttpResponse& response)
{
	HttpRequestHandler* handler = 0;

	std::string requestPath = request.getPath();

	{ SYNC(_lock)

		std::size_t bestLength = 0;

		for(std::map<std::string, HttpRequestHandler*>::iterator iter = _handlers.begin(); iter != _handlers.end(); iter++)
		{
			const std::string& registeredPath = iter -> first;

			// A registered path that already ends in '/' (e.g. root, "/") is itself the separator, so a
			// longer request path sharing that prefix matches without needing another '/' at the boundary.
			bool matches = (requestPath == registeredPath) ||
				(requestPath.size() > registeredPath.size() &&
				 requestPath.compare(0, registeredPath.size(), registeredPath) == 0 &&
				 (registeredPath.back() == '/' || requestPath[registeredPath.size()] == '/'));

			if(matches && registeredPath.size() >= bestLength)
			{
				handler = iter -> second;
				bestLength = registeredPath.size();
			}
		}
	}

	if(handler)
	{
		handler -> handleRequest(request, response);
	}
	else
	{
		response.setStatus(404);
		response.setContentType("text/plain");
		response.setBody("Not Found");
	}
}
