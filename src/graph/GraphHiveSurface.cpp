#include "../agent/ModelContext.hpp"
#include "../util/EventListener.hpp"
#include "GraphException.hpp"
#include "GraphHiveSurface.hpp"
#include "GraphHiveSurfaceListener.hpp"

GraphHiveSurface::GraphHiveSurface(Type type) : _type(type), _hive(0)
{
}

GraphHiveSurface::Type GraphHiveSurface::getType()
{
	return _type;
}

bool GraphHiveSurface::getDefault()
{
	{ SYNC(_lock)

		return _default;
	}
}

void GraphHiveSurface::setDefault(bool isDefault)
{
	{ SYNC(_lock)

		_default = isDefault;
	}
}

void GraphHiveSurface::setHive(Handle<GraphHive> hive)
{
	if(!hive.isValid())
	{
		throw GraphException(GraphException::INVALID_HIVE_HANDLE);
	}

	_hive = hive;
}

GraphHiveSurface::~GraphHiveSurface()
{
}

void GraphHiveSurface::poke(unsigned nodeId, GraphPoke poke)
{
	if(_hive.isValid())
	{
		_hive.getInstance() -> poke(nodeId, poke);
	}
}

std::string GraphHiveSurface::chat(std::string prompt, AgenticHarness::Capability capability, bool newContext,
	unsigned& contextId)
{
	if(!_hive.isValid()) throw GraphException(GraphException::INVALID_HIVE_HANDLE);

	Handle<ModelContext> context(0);

	if(!newContext)
	{
		context = __findChatContext(contextId);

		if(!context.isValid()) throw GraphException(GraphException::INVALID_CHAT_CONTEXT_ID);
	}

	context = _hive.getInstance() -> processAgenticRequest(capability, prompt, context);

	if(!context.isValid()) throw GraphException(GraphException::HIVE_SURFACE_BAD_REQUEST);

	if(newContext)
	{
		{ SYNC(_lock)

			_chatContexts.push_back(context);
		}
	}

	// Taken from the context rather than tracked here, so that a continued conversation is told the id it
	// was already known by and a new one the id it was born with.
	contextId = context.getInstance() -> getId();

	return context.getInstance() -> getLastResponse();
}

void GraphHiveSurface::removeChatContext(unsigned contextId)
{
	// Takes the context off the list so that dropping it, and deleting it where this surface held the last
	// reference to it, happens outside the lock.
	Handle<ModelContext> context(0);

	{ SYNC(_lock)

		bool found = false;

		for(unsigned index = 0; index < _chatContexts.size(); index++)
		{
			if(_chatContexts[index].getInstance() -> getId() != contextId) continue;

			context = _chatContexts[index];

			_chatContexts.erase(_chatContexts.begin() + index);

			found = true;

			break;
		}

		if(!found) throw GraphException(GraphException::INVALID_CHAT_CONTEXT_ID);
	}
}

std::vector<GraphHiveSurface::ChatContext> GraphHiveSurface::getChatContexts()
{
	std::vector<Handle<ModelContext>> chatContexts;

	{ SYNC(_lock)

		chatContexts = _chatContexts;
	}

	std::vector<ChatContext> found;

	// Asked of the contexts outside the lock, as each takes a lock of its own to answer.
	for(Handle<ModelContext>& chatContext : chatContexts)
	{
		ModelContext* context = chatContext.getInstance();

		found.push_back(ChatContext{context -> getId(), context -> getDescription()});
	}

	return found;
}

Handle<ModelContext> GraphHiveSurface::__findChatContext(unsigned contextId)
{
	{ SYNC(_lock)

		for(Handle<ModelContext>& chatContext : _chatContexts)
		{
			if(chatContext.getInstance() -> getId() == contextId) return chatContext;
		}
	}

	return Handle<ModelContext>(0);
}

void GraphHiveSurface::_emitSurfaceChanged()
{
	std::list<EventListener<GraphHiveSurfaceListener>*> listeners = _getListeners();

	for(EventListener<GraphHiveSurfaceListener>* listener : listeners)
	{
		// The listener knows how to get the concrete listener interface.
		GraphHiveSurfaceListener* surfListener = listener -> getListener();

		if(surfListener) surfListener -> hiveSurfaceChanged(Handle<GraphHiveSurface>(this));
	}
}

void GraphHiveSurface::close()
{
	_close();

	decrRef();
}

bool GraphHiveSurface::populateStart(unsigned populateVersion)
{
	{ SYNC(_lock)

		if(_populating) return false;

		_populating = true;
	}

	_populateVersion = populateVersion;

	_populateStart();

	return true;
}

void GraphHiveSurface::populateEnd()
{
	bool notify = false;

	{ SYNC(_lock)

		if(_populating)
		{
			_populating = false;
			notify = true;
		}
		else
		{
			throw GraphException(GraphException::HIVE_SURFACE_BAD_REQUEST);
		}
	}

	if(notify) _populateEnd();
}

bool GraphHiveSurface::isPopulating()
{
	{ SYNC(_lock)

		return _populating;
	}
}

unsigned GraphHiveSurface::getPopulateVersion()
{
	{ SYNC(_lock)

		return _populateVersion;
	}
}

