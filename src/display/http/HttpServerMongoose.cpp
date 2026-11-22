#include "HttpServerMongoose.hpp"

#include "../DisplayException.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpServerMongoosePollThread.hpp"

#include "../../mongoose/mongoose.h"

namespace
{
	/// Largest request body passed on to a handler. Nothing served takes more than a hive definition, which is
	/// far smaller, so anything bigger is refused rather than buffered and parsed.
	const size_t _MAX_BODY_BYTES = 1024 * 1024;

	/**
	 * Get the reason phrase sent after a status code. Only the codes this program sends are named; any other is
	 * sent with an empty phrase, which HTTP allows, as clients go by the code alone.
	 * @param statusCode HTTP status code.
	 */
	const char* _reasonPhrase(unsigned statusCode)
	{
		switch(statusCode)
		{
			case 200: return "OK";
			case 201: return "Created";
			case 204: return "No Content";
			case 400: return "Bad Request";
			case 401: return "Unauthorized";
			case 403: return "Forbidden";
			case 404: return "Not Found";
			case 405: return "Method Not Allowed";
			case 409: return "Conflict";
			case 413: return "Payload Too Large";
			case 429: return "Too Many Requests";
			case 500: return "Internal Server Error";
			case 503: return "Service Unavailable";
		}

		return "";
	}
}

HttpServerMongoose::HttpServerMongoose(std::string address, unsigned port) : HttpServerBase(address, port)
{
	// Mongoose defaults to MG_LL_DEBUG, which logs every accept/close. That is too noisy for something as
	// frequent as data polling, so only let actual errors reach the terminal.
	mg_log_set(MG_LL_ERROR);

	_mgr = new mg_mgr();

	mg_mgr_init(_mgr);
}

HttpServerMongoose::~HttpServerMongoose()
{
	_stop();

	mg_mgr_free(_mgr);

	delete _mgr;
}

void HttpServerMongoose::_start()
{
	std::string url = (getTlsEnabled() ? "https://" : "http://") + getAddress() + ":" + std::to_string(getPort());

	mg_connection* connection = mg_http_listen(_mgr, url.c_str(), __eventHandler, this);

	if(!connection)
	{
		throw DisplayException(DisplayException::HTTP_SERVER_START_FAILED);
	}

	_pollThread = new HttpServerMongoosePollThread(_mgr);

	_pollThread -> start();
}

void HttpServerMongoose::_stop()
{
	if(_pollThread)
	{
		_pollThread -> stop(true);

		delete _pollThread;

		_pollThread = 0;
	}
}

void HttpServerMongoose::__eventHandler(mg_connection* connection, int event, void* eventData)
{
	HttpServerMongoose* self = (HttpServerMongoose*) connection -> fn_data;

	if(event == MG_EV_ACCEPT)
	{
		if(self -> getTlsEnabled())
		{
			mg_tls_opts options = {};

			options.cert = mg_str_n(self -> _getTlsCert().data(), self -> _getTlsCert().size());
			options.key = mg_str_n(self -> _getTlsKey().data(), self -> _getTlsKey().size());

			mg_tls_init(connection, &options);
		}

		return;
	}

	if(event != MG_EV_HTTP_MSG) return;

	mg_http_message* message = (mg_http_message*) eventData;

	if(message -> body.len > _MAX_BODY_BYTES)
	{
		mg_http_reply(connection, 413, "Content-Type: text/plain\r\n", "Request body too large");
		return;
	}

	std::string method(message -> method.buf, message -> method.len);
	std::string path(message -> uri.buf, message -> uri.len);
	std::string query(message -> query.buf, message -> query.len);
	std::string body(message -> body.buf, message -> body.len);

	HttpRequest request(method, path, query, body);

	for(int index = 0; index < MG_MAX_HTTP_HEADERS && message -> headers[index].name.len > 0; index++)
	{
		mg_http_header& header = message -> headers[index];

		request.setHeader(std::string(header.name.buf, header.name.len),
			std::string(header.value.buf, header.value.len));
	}

	char remoteAddress[64];

	mg_snprintf(remoteAddress, sizeof(remoteAddress), "%M", mg_print_ip, &connection -> rem);

	request.setRemoteAddress(remoteAddress);

	HttpResponse response;

	self -> _routeRequest(request, response);

	std::string headers = "Content-Type: " + response.getContentType() + "\r\n";

	for(std::pair<std::string, std::string>& header : response.getHeaders())
	{
		headers += header.first + ": " + header.second + "\r\n";
	}

	std::string responseBody = response.getBody();

	// Sent as raw bytes rather than through mg_http_reply(), whose formatting stops at the first zero byte, so that
	// a binary body such as an image is sent whole.
	mg_printf(connection, "HTTP/1.1 %u %s\r\n%sContent-Length: %lu\r\n\r\n", response.getStatus(),
		_reasonPhrase(response.getStatus()), headers.c_str(), (unsigned long) responseBody.size());
	mg_send(connection, responseBody.data(), responseBody.size());

	// As mg_http_reply() leaves it, to say this connection's response is complete.
	connection -> is_resp = 0;
}
