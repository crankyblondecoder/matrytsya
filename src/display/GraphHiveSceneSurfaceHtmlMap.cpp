#include "GraphHiveSceneSurfaceHtmlMap.hpp"

#include <cstddef>
#include <stdio.h>
#include <stdlib.h>

#include "../agent/AgentException.hpp"
#include "../graph/GraphException.hpp"
#include "../graph/GraphHiveSceneSurface.hpp"
#include "../graph/GraphPoke.hpp"
#include "../rapidjson/document.h"
#include "../thread/thread.hpp"
#include "../thread/ThreadException.hpp"
#include "chatWindowTemplate.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace
{
	/// How long the chat thread waits on a prompt being queued before coming back round to re-check whether it
	/// has been asked to quit, in milliseconds.
	const unsigned _CHAT_QUEUE_WAIT_MS = 250;

	/// How many chat prompts and their replies are kept before the oldest already dealt with are dropped. A
	/// browser normally collects a reply within a poll or two of it landing, so this only bounds what is held
	/// for one that asked and then went away.
	const unsigned _MAX_RETAINED_CHAT_MESSAGES = 32;

	// Quotes a string as a JSON string literal, escaping what JSON does not allow to appear raw within one.
	// Needed by the chat endpoints below, where the text is whatever a model or a browser said rather than the
	// numbers a page draws the surface from.
	std::string jsonString(const std::string& value)
	{
		std::string result = "\"";

		for(char character : value)
		{
			switch(character)
			{
				case '"':  result += "\\\""; break;
				case '\\': result += "\\\\"; break;
				case '\b': result += "\\b"; break;
				case '\f': result += "\\f"; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;

				default:

					// Anything else below a space has no shorthand and may not appear raw, so goes out as an
					// escape. The cast keeps a high byte of a multi-byte UTF-8 sequence from looking negative
					// and being escaped as though it were a control character.
					if((unsigned char) character < 0x20)
					{
						char escape[7];

						snprintf(escape, sizeof(escape), "\\u%04x", (unsigned char) character);

						result += escape;
					}
					else
					{
						result += character;
					}

					break;
			}
		}

		return result + "\"";
	}

	// Plain language for the failures the bound surface can report against a chat, so that the page can say why
	// it could not be serviced rather than showing an error number. AgentException carries its own description,
	// so only the graph side needs one here.
	std::string graphChatErrorDescription(GraphException::Error error)
	{
		switch(error)
		{
			case GraphException::INVALID_HIVE_HANDLE:
				return "This surface is not bound to a hive.";

			case GraphException::AGENTIC_HARNESS_NOT_SET:
				return "The hive has no agentic harness set, so there is no model to chat with.";

			case GraphException::INVALID_CHAT_CONTEXT_ID:
				return "That conversation is no longer held by this surface.";

			case GraphException::HIVE_SURFACE_BAD_REQUEST:
				return "The surface refused the chat request.";

			default:
				return "The chat could not be serviced.";
		}
	}

	// Replaces every occurrence of a page template's placeholder. Searching on from the end of what was just put
	// in rather than from where it went keeps a value that itself contains the placeholder from being expanded
	// again for ever.
	void replacePlaceholder(std::string& page, const std::string& placeholder, const std::string& value)
	{
		std::size_t position = page.find(placeholder);

		while(position != std::string::npos)
		{
			page.replace(position, placeholder.size(), value);

			position = page.find(placeholder, position + value.size());
		}
	}
}

GraphHiveSceneSurfaceHtmlMap::GraphHiveSceneSurfaceHtmlMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface,
	std::string path) :
	GraphHiveSurfaceHttpMap(httpServer, surface, path), _sceneSurface(&surface)
{
	surface.addListener(*this);
}

GraphHiveSceneSurfaceHtmlMap::~GraphHiveSceneSurfaceHtmlMap()
{
	// Unbind before anything else, so the surface releases its ref on this and no further event can reach a
	// map that is already tearing down.
	_getSceneSurface().removeListener(*this);

	bool chatThreadRunning;

	{ SYNC(_lock)

		chatThreadRunning = _chatThreadRunning;
	}

	// A chat already with the model cannot be called back, so this waits for it to be answered before the chat
	// thread notices it has been asked to quit, and forces the thread down if it takes too long about it. It has
	// to happen here rather than being left to the base class, whose destructor cannot stop a running thread, as
	// the thread reads this map's state right up until it stops.
	if(chatThreadRunning) stop(true);
}

void GraphHiveSceneSurfaceHtmlMap::setPollInterval(unsigned pollIntervalMs)
{
	{ SYNC(_lock)

		_pollIntervalMs = pollIntervalMs;
	}
}

void GraphHiveSceneSurfaceHtmlMap::setChatCapability(AgenticHarness::Capability capability)
{
	{ SYNC(_lock)

		_chatCapability = capability;
	}
}

GraphHiveSceneSurface& GraphHiveSceneSurfaceHtmlMap::_getSceneSurface()
{
	return *_sceneSurface.getInstance();
}

std::string GraphHiveSceneSurfaceHtmlMap::_getBasePath()
{
	std::string base = getPath();

	while(!base.empty() && base.back() == '/') base.pop_back();

	return base;
}

void GraphHiveSceneSurfaceHtmlMap::_renderPageTemplate(const std::string& pageTemplate, HttpResponse& response)
{
	std::string page = pageTemplate;

	std::string title = _sceneSurface.getInstance() -> getName();
	unsigned pollIntervalMs;

	{ SYNC(_lock)

		pollIntervalMs = _pollIntervalMs;
	}

	if(title.empty()) title = "Graph Hive Scene Surface";

	// Put the chat window in before anything else, so that a placeholder appearing within it is filled in too
	// rather than being left in the page as it stands.
	replacePlaceholder(page, "%CHAT_STYLE%", chatWindowStyle);
	replacePlaceholder(page, "%CHAT_MARKUP%", chatWindowMarkup);
	replacePlaceholder(page, "%CHAT_SCRIPT%", chatWindowScript);

	replacePlaceholder(page, "%TITLE%", title);
	replacePlaceholder(page, "%POLL_INTERVAL_MS%", std::to_string(pollIntervalMs));

	response.setContentType("text/html");
	response.setBody(page);
}

void GraphHiveSceneSurfaceHtmlMap::_serveData(HttpRequest& request, HttpResponse& response)
{
	std::string base = _getBasePath();

	if(request.getPath() == base + "/revision")
	{
		__serveRevision(response);

		return;
	}

	if(request.getPath() == base + "/poke")
	{
		__servePoke(request, response);

		return;
	}

	if(request.getPath() == base + "/chat")
	{
		__serveChat(request, response);

		return;
	}

	if(request.getPath() == base + "/chat/message")
	{
		__serveChatMessage(request, response);

		return;
	}

	if(request.getPath() == base + "/chat/contexts")
	{
		__serveChatContexts(response);

		return;
	}

	if(request.getPath() == base + "/chat/removeContext")
	{
		__serveChatContextRemove(request, response);

		return;
	}

	// Not one of the endpoints every page served by this map carries, so it belongs to however the subclass
	// draws the surface.
	_serveMapData(request, response);
}

void GraphHiveSceneSurfaceHtmlMap::__serveRevision(HttpResponse& response)
{
	unsigned revision;

	{ SYNC(_lock)

		revision = _revision;
	}

	response.setContentType("application/json");
	response.setBody("{\"revision\":" + std::to_string(revision) + "}");
}

void GraphHiveSceneSurfaceHtmlMap::__servePoke(HttpRequest& request, HttpResponse& response)
{
	std::string nodeIdParam = request.getQueryParam("nodeId");
	std::string chunkIdParam = request.getQueryParam("chunkId");

	// Both are required: the node id routes the poke to the owning node, the chunk id identifies which of
	// that node's chunks was poked. Neither is unique enough to stand alone.
	if(nodeIdParam.empty() || chunkIdParam.empty())
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing nodeId or chunkId\"}");

		return;
	}

	unsigned nodeId = (unsigned) strtoul(nodeIdParam.c_str(), 0, 10);
	unsigned chunkId = (unsigned) strtoul(chunkIdParam.c_str(), 0, 10);

	GraphPoke::PokeType type = GraphPoke::PokeType::HIT;

	std::string typeParam = request.getQueryParam("type");

	if(typeParam == "hoverEnter") type = GraphPoke::PokeType::HOVER_ENTER;
	else if(typeParam == "hoverLeave") type = GraphPoke::PokeType::HOVER_LEAVE;

	GraphPoke::PokeData data{};

	_sceneSurface.getInstance() -> poke(nodeId, GraphPoke(type, data, chunkId));

	response.setContentType("application/json");
	response.setBody("{\"ok\":true}");
}

void GraphHiveSceneSurfaceHtmlMap::__serveChat(HttpRequest& request, HttpResponse& response)
{
	rapidjson::Document document;

	document.Parse(request.getBody().c_str());

	if(document.HasParseError() || !document.IsObject() || !document.HasMember("prompt") ||
		!document["prompt"].IsString() || document["prompt"].GetStringLength() == 0)
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing prompt\"}");

		return;
	}

	// A conversation to carry on is named by its id being there at all; ids come from the contexts themselves
	// and start at zero, so no value of one can stand for "start a fresh conversation".
	bool newContext = !document.HasMember("contextId") || !document["contextId"].IsUint();
	unsigned contextId = newContext ? 0 : document["contextId"].GetUint();

	// Queueing a prompt that nothing would ever pick up would leave the page waiting on a reply that could not
	// come, so the thread has to be up before the prompt is taken on.
	if(!__startChatThread())
	{
		response.setStatus(503);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"the chat thread could not be started\"}");

		return;
	}

	unsigned messageId = __queueChatMessage(document["prompt"].GetString(), newContext, contextId);

	response.setContentType("application/json");
	response.setBody("{\"messageId\":" + std::to_string(messageId) + ",\"state\":\"pending\"}");
}

void GraphHiveSceneSurfaceHtmlMap::__serveChatMessage(HttpRequest& request, HttpResponse& response)
{
	std::string messageIdParam = request.getQueryParam("messageId");

	if(messageIdParam.empty())
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing messageId\"}");

		return;
	}

	unsigned messageId = (unsigned) strtoul(messageIdParam.c_str(), 0, 10);

	bool found = false;
	ChatMessage message{};

	_chatMessagesCond.lockMutex();

	for(ChatMessage& queued : _chatMessages)
	{
		if(queued.id != messageId) continue;

		message = queued;
		found = true;

		break;
	}

	_chatMessagesCond.unlockMutex();

	if(!found)
	{
		// Either it was never accepted, or it was answered so long ago that it has since been dropped to keep
		// the retained messages bounded.
		response.setStatus(404);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"unknown messageId\"}");

		return;
	}

	std::string json = "{\"messageId\":" + std::to_string(message.id);

	switch(message.state)
	{
		case ChatState::ANSWERED:

			// The context id only means anything once the surface has answered, which is the whole point of
			// sending it back: it is how a chat that started a conversation tells the page what to carry it on
			// with.
			json += ",\"state\":\"answered\",\"contextId\":" + std::to_string(message.contextId) +
				",\"reply\":" + jsonString(message.reply);

			break;

		case ChatState::FAILED:

			json += ",\"state\":\"failed\",\"error\":" + jsonString(message.error);

			break;

		default:

			json += ",\"state\":\"pending\"";

			break;
	}

	response.setContentType("application/json");
	response.setBody(json + "}");
}

void GraphHiveSceneSurfaceHtmlMap::__serveChatContexts(HttpResponse& response)
{
	std::vector<GraphHiveSurface::ChatContext> contexts = _sceneSurface.getInstance() -> getChatContexts();

	std::string json = "{\"contexts\":[";

	for(std::size_t index = 0; index < contexts.size(); index++)
	{
		if(index > 0) json += ",";

		json += "{\"id\":" + std::to_string(contexts[index].id) +
			",\"description\":" + jsonString(contexts[index].description) + "}";
	}

	response.setContentType("application/json");
	response.setBody(json + "]}");
}

void GraphHiveSceneSurfaceHtmlMap::__serveChatContextRemove(HttpRequest& request, HttpResponse& response)
{
	std::string contextIdParam = request.getQueryParam("contextId");

	if(contextIdParam.empty())
	{
		response.setStatus(400);
		response.setContentType("application/json");
		response.setBody("{\"error\":\"missing contextId\"}");

		return;
	}

	unsigned contextId = (unsigned) strtoul(contextIdParam.c_str(), 0, 10);

	try
	{
		_sceneSurface.getInstance() -> removeChatContext(contextId);
	}
	catch(GraphException& exception)
	{
		response.setStatus(404);
		response.setContentType("application/json");
		response.setBody("{\"error\":" + jsonString(graphChatErrorDescription(exception.getError())) + "}");

		return;
	}

	response.setContentType("application/json");
	response.setBody("{\"ok\":true}");
}

bool GraphHiveSceneSurfaceHtmlMap::__startChatThread()
{
	{ SYNC(_lock)

		// A thread can only ever be started once, so a start that failed is not tried again. What matters to
		// the caller either way is whether there is a thread running to pick a queued prompt up.
		if(_chatThreadStarted) return _chatThreadRunning;

		_chatThreadStarted = true;
	}

	bool running = false;

	// Outside the lock, both because starting a thread is not something to do while holding one and because the
	// thread being started takes that same lock as soon as it has a prompt to service. The flag above is set
	// first, so a chat arriving meanwhile cannot try to start the thread a second time.
	try
	{
		running = start();
	}
	catch(ThreadException& exception)
	{
		// Reported to the browser as the chat being unavailable rather than let out of an HTTP handler, which
		// is called from the HTTP server's own event loop and has nowhere to report it to.
		running = false;
	}

	{ SYNC(_lock)

		_chatThreadRunning = running;
	}

	return running;
}

unsigned GraphHiveSceneSurfaceHtmlMap::__queueChatMessage(std::string prompt, bool newContext, unsigned contextId)
{
	unsigned messageId;

	_chatMessagesCond.lockMutex();

	messageId = _nextChatMessageId++;

	// Make room by dropping the oldest messages that have already been dealt with. Ones still queued or with the
	// model are left alone however many there are, since dropping one would lose a reply that is still coming.
	while(_chatMessages.size() >= _MAX_RETAINED_CHAT_MESSAGES)
	{
		bool dropped = false;

		for(unsigned index = 0; index < _chatMessages.size(); index++)
		{
			if(_chatMessages[index].state == ChatState::PENDING ||
				_chatMessages[index].state == ChatState::SERVICING)
			{
				continue;
			}

			_chatMessages.erase(_chatMessages.begin() + index);

			dropped = true;

			break;
		}

		if(!dropped) break;
	}

	_chatMessages.push_back(ChatMessage{messageId, contextId, newContext, prompt, "", "", ChatState::PENDING});

	_chatMessagesCond.signal();

	_chatMessagesCond.unlockMutex();

	return messageId;
}

bool GraphHiveSceneSurfaceHtmlMap::__takeNextChatMessage(ChatMessage& message)
{
	bool taken = false;

	try
	{
		_chatMessagesCond.lockMutex();
	}
	catch(ThreadException& exception)
	{
		return false;
	}

	try
	{
		for(ChatMessage& queued : _chatMessages)
		{
			if(queued.state != ChatState::PENDING) continue;

			queued.state = ChatState::SERVICING;

			message = queued;
			taken = true;

			break;
		}

		// Waits with a timeout rather than indefinitely, so that a quit asked for while nothing is queued is
		// picked up by the loop in threadEntry() even if it was asked for between that loop's check and this
		// wait.
		if(!taken) _chatMessagesCond.waitTimeout(_CHAT_QUEUE_WAIT_MS);
	}
	catch(ThreadException& exception)
	{
		// Nothing was taken, so the caller simply comes back round after re-checking whether it should quit.
	}

	_chatMessagesCond.unlockMutex();

	return taken;
}

void GraphHiveSceneSurfaceHtmlMap::__completeChatMessage(unsigned messageId, unsigned contextId, std::string reply,
	std::string error)
{
	_chatMessagesCond.lockMutex();

	for(ChatMessage& message : _chatMessages)
	{
		if(message.id != messageId) continue;

		message.contextId = contextId;
		message.reply = reply;
		message.error = error;
		message.state = error.empty() ? ChatState::ANSWERED : ChatState::FAILED;

		break;
	}

	_chatMessagesCond.unlockMutex();
}

void GraphHiveSceneSurfaceHtmlMap::threadEntry()
{
	while(!_getQuit())
	{
		ChatMessage message;

		if(!__takeNextChatMessage(message)) continue;

		AgenticHarness::Capability capability;

		{ SYNC(_lock)

			capability = _chatCapability;
		}

		// The surface reports the conversation the chat was serviced in through this, which is the only way to
		// learn the id of one the chat itself started.
		unsigned contextId = message.contextId;

		std::string reply;
		std::string error;

		try
		{
			reply = _sceneSurface.getInstance() -> chat(message.prompt, capability, message.newContext, contextId);
		}
		catch(GraphException& exception)
		{
			error = graphChatErrorDescription(exception.getError());
		}
		catch(AgentException& exception)
		{
			error = exception.getDescription();
		}

		__completeChatMessage(message.id, contextId, reply, error);
	}
}

void GraphHiveSceneSurfaceHtmlMap::_quitRequested()
{
	// Wakes the chat thread if it is waiting on a prompt being queued. One already with the model is not
	// interrupted; the thread only gets back to its quit check once the model has answered it.
	_chatMessagesCond.lockMutex();

	_chatMessagesCond.broadcast();

	_chatMessagesCond.unlockMutex();
}

void GraphHiveSceneSurfaceHtmlMap::hiveSurfaceChanged(EventEmitter<GraphHiveSurfaceListener>& emitter)
{
	{ SYNC(_lock)

		_revision++;
	}
}

void GraphHiveSceneSurfaceHtmlMap::populateEventListenerHandle(CastHandle<GraphHiveSurfaceListener>& handle)
{
	handle = CastHandle<GraphHiveSurfaceListener>(this, this);
}
