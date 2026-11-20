#include "HttpServerBase.hpp"

#include "../../thread/thread.hpp"
#include "../DisplayException.hpp"
#include "HttpRequest.hpp"
#include "HttpRequestHandler.hpp"
#include "HttpResponse.hpp"

#include <fstream>
#include <sstream>

namespace
{
	// Reads a whole file, throwing if it cannot be read or holds nothing, as an empty certificate or key would
	// only fail later, on the first connection, far from the option that named it.
	std::string readPemFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);

		if(!file) throw DisplayException(DisplayException::TLS_CERT_UNREADABLE);

		std::ostringstream contents;

		contents << file.rdbuf();

		if(!file.good() && !file.eof()) throw DisplayException(DisplayException::TLS_CERT_UNREADABLE);

		std::string pem = contents.str();

		if(pem.empty()) throw DisplayException(DisplayException::TLS_CERT_UNREADABLE);

		return pem;
	}
}

HttpServerBase::HttpServerBase(std::string address, unsigned port) : _address{address}, _port{port}
{
}

HttpServerBase::~HttpServerBase()
{
	// The key is overwritten rather than just released, so that it is not left lying in freed memory.
	volatile char* key = (volatile char*) _tlsKey.data();

	for(std::size_t index = 0; index < _tlsKey.size(); index++) key[index] = 0;
}

void HttpServerBase::setTls(std::string certPath, std::string keyPath)
{
	_tlsCert = readPemFile(certPath);
	_tlsKey = readPemFile(keyPath);
}

bool HttpServerBase::getTlsEnabled()
{
	return !_tlsCert.empty();
}

const std::string& HttpServerBase::_getTlsCert()
{
	return _tlsCert;
}

const std::string& HttpServerBase::_getTlsKey()
{
	return _tlsKey;
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

std::string HttpServerBase::getAddress()
{
	return _address;
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

		// Held from here so that a handler removed by another thread, e.g. when a hive is deleted, is not
		// destroyed while this request is still inside it. One that is already being destroyed is answered as
		// though it were never there.
		if(handler && !handler -> retainHandler()) handler = 0;
	}

	if(handler)
	{
		try
		{
			handler -> handleRequest(request, response);
		}
		catch(...)
		{
			handler -> releaseHandler();
			throw;
		}

		handler -> releaseHandler();
	}
	else
	{
		response.setStatus(404);
		response.setContentType("text/plain");
		response.setBody("Not Found");
	}
}
