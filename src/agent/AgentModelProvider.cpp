#include "AgentModelProvider.hpp"

#include "AgentException.hpp"

#include "../mongoose/mongoose.h"

namespace
{
	/// Milliseconds to wait for a connection to a provider before giving up.
	const uint64_t CONNECT_TIMEOUT_MS = 5000;

	/// State tracked by __checkConnectionEventHandler while a connection check is in progress.
	struct ConnectionCheckState
	{
		bool connected = false;
		bool failed = false;
	};

	/// State tracked by __httpGetEventHandler while a GET request is in progress.
	struct HttpGetState
	{
		/// Full URL of the request, kept for building the request line on MG_EV_CONNECT.
		std::string url;
		bool done = false;
		bool failed = false;
		std::string body;
	};
}

AgentModelProvider::~AgentModelProvider()
{
}

AgentModelProvider::AgentModelProvider()
{
}

void AgentModelProvider::_addModel(AgentModel model)
{
	_models.push_back(model);
}

std::vector<AgentModel> AgentModelProvider::getModels()
{
	return _models;
}

void AgentModelProvider::refreshModels()
{
	_models.clear();

	_populateModels();
}

void AgentModelProvider::_checkConnection(std::string url)
{
	// Mongoose defaults to MG_LL_DEBUG, which logs every connection attempt. Only let actual errors
	// reach the terminal.
	mg_log_set(MG_LL_ERROR);

	mg_mgr mgr;

	mg_mgr_init(&mgr);

	ConnectionCheckState state;

	mg_connection* connection = mg_connect(&mgr, url.c_str(), __checkConnectionEventHandler, &state);

	if(connection)
	{
		uint64_t start = mg_millis();

		while(!state.connected && !state.failed && (mg_millis() - start) < CONNECT_TIMEOUT_MS)
		{
			mg_mgr_poll(&mgr, 50);
		}
	}

	mg_mgr_free(&mgr);

	if(!connection || !state.connected)
	{
		throw AgentException(AgentException::CONNECTION_FAILED);
	}
}

void AgentModelProvider::__checkConnectionEventHandler(mg_connection* connection, int event, void* eventData)
{
	ConnectionCheckState* state = (ConnectionCheckState*) connection -> fn_data;

	if(event == MG_EV_CONNECT)
	{
		state -> connected = true;
	}
	else if(event == MG_EV_ERROR || event == MG_EV_CLOSE)
	{
		state -> failed = true;
	}
}

std::string AgentModelProvider::_httpGet(std::string url)
{
	// Mongoose defaults to MG_LL_DEBUG, which logs every connection attempt. Only let actual errors
	// reach the terminal.
	mg_log_set(MG_LL_ERROR);

	mg_mgr mgr;

	mg_mgr_init(&mgr);

	HttpGetState state;

	state.url = url;

	mg_connection* connection = mg_http_connect(&mgr, state.url.c_str(), __httpGetEventHandler, &state);

	if(connection)
	{
		uint64_t start = mg_millis();

		while(!state.done && !state.failed && (mg_millis() - start) < CONNECT_TIMEOUT_MS)
		{
			mg_mgr_poll(&mgr, 50);
		}
	}

	mg_mgr_free(&mgr);

	if(!connection || !state.done || state.failed)
	{
		throw AgentException(AgentException::CONNECTION_FAILED);
	}

	return state.body;
}

void AgentModelProvider::__httpGetEventHandler(mg_connection* connection, int event, void* eventData)
{
	HttpGetState* state = (HttpGetState*) connection -> fn_data;

	if(event == MG_EV_CONNECT)
	{
		mg_str host = mg_url_host(state -> url.c_str());

		mg_printf(connection, "GET %s HTTP/1.1\r\nHost: %.*s\r\nConnection: close\r\n\r\n", mg_url_uri(state -> url.c_str()), (int) host.len, host.buf);
	}
	else if(event == MG_EV_HTTP_MSG)
	{
		mg_http_message* message = (mg_http_message*) eventData;

		state -> body = std::string(message -> body.buf, message -> body.len);
		state -> done = true;

		connection -> is_closing = 1;
	}
	else if(event == MG_EV_ERROR || event == MG_EV_CLOSE)
	{
		state -> failed = true;
	}
}
