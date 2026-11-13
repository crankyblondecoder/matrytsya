#include "ModelProvider.hpp"

#include "AgentException.hpp"
#include "Model.hpp"
#include "ModelContext.hpp"
#include "ModelRequest.hpp"
#include "../thread/ThreadBase.hpp"

#include "../mongoose/mongoose.h"

#include <fstream>
#include <sstream>

namespace
{
	/// Milliseconds to wait for a connection to a provider before giving up.
	const uint64_t CONNECT_TIMEOUT_MS = 5000;

	/// Milliseconds to wait for a provider to service a request before giving up.
	const uint64_t REQUEST_TIMEOUT_MS = 300000;

	/// Bundle of the certificate authorities trusted to vouch for a provider served over HTTPS.
	const char* const CA_BUNDLE_PATH = "/etc/ssl/certs/ca-certificates.crt";

	/**
	 * Get the trusted certificate authority bundle, reading it on first use.
	 * @returns The PEM text of the bundle. Empty when it could not be read.
	 * @note Kept for the life of the process: every request to a provider over HTTPS needs it, and it is
	 *       large enough that reading it again for each of them would be a waste.
	 */
	const std::string& __caBundle()
	{
		// Initialised once, safely, however many threads first ask for it at the same time.
		static const std::string bundle = []()
		{
			std::ifstream file(CA_BUNDLE_PATH, std::ios::binary);

			std::stringstream contents;

			contents << file.rdbuf();

			return contents.str();
		}();

		return bundle;
	}

	/**
	 * Start TLS on a connection that has just been made, if its URL asks for it.
	 * @param connection Connection that has just been made.
	 * @param url URL the connection was made to.
	 * @note Fails the connection when no certificate authorities could be read, rather than going ahead
	 *       without verifying the server, as a request may carry credentials.
	 */
	void __startTlsIfRequired(mg_connection* connection, const std::string& url)
	{
		if(!mg_url_is_ssl(url.c_str())) return;

		const std::string& bundle = __caBundle();

		if(bundle.empty())
		{
			mg_error(connection, "no trusted certificate authorities could be read from %s", CA_BUNDLE_PATH);

			return;
		}

		mg_tls_opts options = {};

		options.ca = mg_str_n(bundle.data(), bundle.size());
		options.name = mg_url_host(url.c_str());

		mg_tls_init(connection, &options);
	}

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

		/// Provider specific header lines, kept for writing on MG_EV_CONNECT.
		std::string headers;
		bool done = false;
		bool failed = false;
		std::string body;
	};

	/// State tracked by __httpPostEventHandler while a POST request is in progress.
	struct HttpPostState
	{
		/// Full URL of the request, kept for building the request line on MG_EV_CONNECT.
		std::string url;

		/// Provider specific header lines, kept for writing on MG_EV_CONNECT.
		std::string headers;

		/// Body to send, kept for writing on MG_EV_CONNECT.
		std::string requestBody;
		bool done = false;
		bool failed = false;
		std::string body;
	};
}

ModelProvider::~ModelProvider()
{
}

ModelProvider::ModelProvider()
{
}

void ModelProvider::_addModel(Handle<Model> model)
{
	_models.push_back(model);
}

std::vector<Handle<Model>> ModelProvider::getModels()
{
	return _models;
}

void ModelProvider::refreshModels()
{
	_models.clear();

	_populateModels();
}

std::string ModelProvider::processRequest(Handle<Model> model, ModelRequest& request)
{
	Handle<ModelContext> contextHandle = request.getContext();

	// Held by the handle above for the whole of this call, so the raw pointer cannot outlive the context.
	ModelContext* context = contextHandle.getInstance();

	// Turns away a second request in the same context rather than letting two of them interleave their
	// prompts and tool calls in one conversation. Given up when it leaves scope, however this returns.
	ModelContext::RequestClaim claim(*context);

	std::vector<ModelContext::ToolCallRound> toolCallRounds;

	std::string response = _processRequest(model, request, toolCallRounds);

	// The prompt has been processed, so it, the tool calls made along the way and the response become part
	// of the conversation that the next request made in the same context is answered against.
	context -> addChatExchange(request.getPrompt(), toolCallRounds, response);

	return response;
}

void ModelProvider::_checkConnection(std::string url)
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

		while(!state.connected && !state.failed && !ThreadBase::currentThreadStopping()
			&& (mg_millis() - start) < CONNECT_TIMEOUT_MS)
		{
			mg_mgr_poll(&mgr, 50);
		}
	}

	mg_mgr_free(&mgr);

	// Reported apart from a failure, so that a check dropped because the application is going down is not
	// taken for a provider that could not be reached.
	if(!state.connected && ThreadBase::currentThreadStopping())
	{
		throw AgentException(AgentException::REQUEST_ABORTED);
	}

	if(!connection || !state.connected)
	{
		throw AgentException(AgentException::CONNECTION_FAILED);
	}
}

void ModelProvider::__checkConnectionEventHandler(mg_connection* connection, int event, void* eventData)
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

std::string ModelProvider::_truncateDetail(const std::string& text)
{
	if(text.size() <= MAX_ERROR_DETAIL_LENGTH) return text;

	return text.substr(0, MAX_ERROR_DETAIL_LENGTH) + "...";
}

std::string ModelProvider::_unreadableResponseDetail(const std::string& body)
{
	if(body.empty()) return "The response was empty.";

	return "The response began: " + _truncateDetail(body);
}

std::string ModelProvider::_requestHeaders()
{
	return "";
}

std::string ModelProvider::_httpGet(std::string url)
{
	// Mongoose defaults to MG_LL_DEBUG, which logs every connection attempt. Only let actual errors
	// reach the terminal.
	mg_log_set(MG_LL_ERROR);

	mg_mgr mgr;

	mg_mgr_init(&mgr);

	HttpGetState state;

	state.url = url;
	state.headers = _requestHeaders();

	mg_connection* connection = mg_http_connect(&mgr, state.url.c_str(), __httpGetEventHandler, &state);

	if(connection)
	{
		uint64_t start = mg_millis();

		while(!state.done && !state.failed && !ThreadBase::currentThreadStopping()
			&& (mg_millis() - start) < CONNECT_TIMEOUT_MS)
		{
			mg_mgr_poll(&mgr, 50);
		}
	}

	mg_mgr_free(&mgr);

	// Reported apart from a failure, so that a request dropped because the application is going down is not
	// taken for a provider that could not be reached.
	if(!state.done && ThreadBase::currentThreadStopping())
	{
		throw AgentException(AgentException::REQUEST_ABORTED);
	}

	if(!connection || !state.done || state.failed)
	{
		throw AgentException(AgentException::CONNECTION_FAILED);
	}

	return state.body;
}

void ModelProvider::__httpGetEventHandler(mg_connection* connection, int event, void* eventData)
{
	HttpGetState* state = (HttpGetState*) connection -> fn_data;

	if(event == MG_EV_CONNECT)
	{
		mg_str host = mg_url_host(state -> url.c_str());

		__startTlsIfRequired(connection, state -> url);

		mg_printf(connection, "GET %s HTTP/1.1\r\nHost: %.*s\r\n%sConnection: close\r\n\r\n", mg_url_uri(state -> url.c_str()), (int) host.len, host.buf, state -> headers.c_str());
	}
	else if(event == MG_EV_HTTP_MSG)
	{
		mg_http_message* message = (mg_http_message*) eventData;

		state -> body = std::string(message -> body.buf, message -> body.len);
		state -> done = true;

		connection -> is_closing = 1;
	}
	// Mongoose closes the connection in the same poll pass that delivered the response, so a close
	// after the body has arrived is the normal end of a successful request, not a failure.
	else if(event == MG_EV_ERROR || (event == MG_EV_CLOSE && !state -> done))
	{
		state -> failed = true;
	}
}

std::string ModelProvider::_httpPost(std::string url, std::string body)
{
	// Mongoose defaults to MG_LL_DEBUG, which logs every connection attempt. Only let actual errors
	// reach the terminal.
	mg_log_set(MG_LL_ERROR);

	mg_mgr mgr;

	mg_mgr_init(&mgr);

	HttpPostState state;

	state.url = url;
	state.requestBody = body;
	state.headers = _requestHeaders();

	mg_connection* connection = mg_http_connect(&mgr, state.url.c_str(), __httpPostEventHandler, &state);

	if(connection)
	{
		uint64_t start = mg_millis();

		while(!state.done && !state.failed && !ThreadBase::currentThreadStopping()
			&& (mg_millis() - start) < REQUEST_TIMEOUT_MS)
		{
			mg_mgr_poll(&mgr, 50);
		}
	}

	mg_mgr_free(&mgr);

	// Reported apart from a failure, so that a request dropped because the application is going down is not
	// taken for a provider that could not be reached. This matters far more here than it does for a plain
	// GET, as this is the wait that can otherwise hold a thread for the whole of a model's inference.
	if(!state.done && ThreadBase::currentThreadStopping())
	{
		throw AgentException(AgentException::REQUEST_ABORTED);
	}

	if(!connection || !state.done || state.failed)
	{
		throw AgentException(AgentException::CONNECTION_FAILED);
	}

	return state.body;
}

void ModelProvider::__httpPostEventHandler(mg_connection* connection, int event, void* eventData)
{
	HttpPostState* state = (HttpPostState*) connection -> fn_data;

	if(event == MG_EV_CONNECT)
	{
		mg_str host = mg_url_host(state -> url.c_str());

		__startTlsIfRequired(connection, state -> url);

		mg_printf(connection, "POST %s HTTP/1.1\r\nHost: %.*s\r\n%sContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", mg_url_uri(state -> url.c_str()), (int) host.len, host.buf, state -> headers.c_str(), (int) state -> requestBody.size());

		mg_send(connection, state -> requestBody.data(), state -> requestBody.size());
	}
	else if(event == MG_EV_HTTP_MSG)
	{
		mg_http_message* message = (mg_http_message*) eventData;

		state -> body = std::string(message -> body.buf, message -> body.len);
		state -> done = true;

		connection -> is_closing = 1;
	}
	// Mongoose closes the connection in the same poll pass that delivered the response, so a close
	// after the body has arrived is the normal end of a successful request, not a failure.
	else if(event == MG_EV_ERROR || (event == MG_EV_CLOSE && !state -> done))
	{
		state -> failed = true;
	}
}
