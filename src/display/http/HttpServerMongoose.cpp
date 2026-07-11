#include "HttpServerMongoose.hpp"

#include "../DisplayException.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpServerMongoosePollThread.hpp"

#include "../../mongoose/mongoose.h"

HttpServerMongoose::HttpServerMongoose(unsigned port) : HttpServerBase(port)
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
	std::string url = "http://127.0.0.1:" + std::to_string(getPort());

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
	if(event != MG_EV_HTTP_MSG) return;

	mg_http_message* message = (mg_http_message*) eventData;

	HttpServerMongoose* self = (HttpServerMongoose*) connection -> fn_data;

	std::string method(message -> method.buf, message -> method.len);
	std::string path(message -> uri.buf, message -> uri.len);
	std::string query(message -> query.buf, message -> query.len);
	std::string body(message -> body.buf, message -> body.len);

	HttpRequest request(method, path, query, body);
	HttpResponse response;

	self -> _routeRequest(request, response);

	std::string headers = "Content-Type: " + response.getContentType() + "\r\n";

	mg_http_reply(connection, (int) response.getStatus(), headers.c_str(), "%s", response.getBody().c_str());
}
