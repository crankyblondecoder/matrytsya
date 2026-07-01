#include <stdexcept>

#include "../thread/ThreadException.hpp"
#include "../thread/ThreadPool.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphNode.hpp"

GraphHive::~GraphHive()
{
}

GraphHive::GraphHive(unsigned numThreads)
{
	try
	{
		_threadPool = new ThreadPool(numThreads);

		bool isActive = _threadPool -> waitOnBecomingActive();

		if(!isActive)
		{
			_threadPool -> shutdown();
			_threadPool = 0;

			std::string msg = "Critical error: thread pool did not become active.";
			throw std::runtime_error(msg);
		}
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: thread pool creation failed -> " +
			ex.getSubsystemErrorString();

		if(_threadPool)
		{
			_threadPool -> shutdown();
			_threadPool = 0;
		}

		throw std::runtime_error(msg);
	}

	_active = true;
}

unsigned GraphHive::actionActive(GraphAction* action)
{
	unsigned retHandle = 0;

	{ SYNC(_lock)

		bool foundSlot = false;

		for(unsigned index = 0; index < _activeActions.size(); index++)
		{
			if(!_activeActions[index])
			{
				_activeActions[index] = action;
				retHandle = index;
				foundSlot = true;
				break;
			}
		}

		if(!foundSlot)
		{
			// No spare slot available.
			_activeActions.push_back(action);

			retHandle = _activeActions.size() - 1;
		}
	}

	_noActionsActiveCond.lockMutex();
	_activeActionCount++;
	_noActionsActiveCond.unlockMutex();

	return retHandle;
}

void GraphHive::actionInactive(unsigned handle)
{
	bool removed = false;

	{ SYNC(_lock)

		if(handle < _activeActions.size() && _activeActions[handle])
		{
			_activeActions[handle] = 0;
			removed = true;
		}
	}

	if(removed)
	{
		_noActionsActiveCond.lockMutex();

		_activeActionCount--;

		if(_activeActionCount == 0) _noActionsActiveCond.broadcast();

		_noActionsActiveCond.unlockMutex();
	}
}

void GraphHive::waitOnNoActionsActive(unsigned timeOut)
{
	// This is required so that this can't be deleted before the condition can be completed.
	if(incrRef())
	{
		try
		{
			_noActionsActiveCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			decrRef();
			return;
		}

		if(_activeActionCount != 0)
		{
			try
			{
				if(timeOut > 0)
				{
					unsigned loopLimit = 5;
					unsigned effTimeout = timeOut / loopLimit;
					if(effTimeout < 1) effTimeout = 1;

					while(_activeActionCount != 0 && loopLimit--) _noActionsActiveCond.waitTimeout(effTimeout);
				}
				else
				{
					while(_activeActionCount != 0) _noActionsActiveCond.wait();
				}
			}
			catch(ThreadException& ex)
			{
				_noActionsActiveCond.unlockMutex();
				decrRef();
				return;
			}
		}

		// Thread is no longer waiting.
		_noActionsActiveCond.unlockMutex();

		decrRef();
	}
}

void GraphHive::teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation)
{
	GraphHiveCollection* curCollection = 0;

	{ SYNC(_lock)

		if(!_active || !_collection)
		{
			throw GraphException(GraphException::ACTION_TELEPORT_FAILED);
		}

		curCollection = _collection;
	}

	curCollection -> teleportAction(actionPayload, nodeLocation);
}

void GraphHive::setHiveCollection(GraphHiveCollection* collection)
{
	{ SYNC(_lock)

		if(_active) _collection = collection;
	}
}

bool GraphHive::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	{ SYNC(_lock)

		if(!_active)
		{
			delete workUnit;
			return false;
		}

		return _threadPool -> executeWorkUnit(workUnit);
	}
}

void GraphHive::shutdown()
{
	{ SYNC(_lock)

		// After construction, assume that the active flag can only be set to false by shutdown.
		if(!_active) return;

		_active = false;
	}

	for(GraphNode* node : _nodes)
	{
		if(node)
		{
			node -> decouple();
			node -> decrRef();
		}
	}

	if(_threadPool)
	{
		_threadPool -> shutdown();
		delete _threadPool;
		_threadPool = 0;
	}

	// Allow this to be deleted.
	decrRef();
}

int GraphHive::addNode(GraphNode* node)
{
	int retIndex = -1;

	{ SYNC(_lock)

		if(_active)
		{
			for(unsigned index = 0; index < _nodes.size(); index++)
			{
				if(!_nodes[index])
				{
					_nodes[index] = node;
					retIndex = index;
					break;
				}
			}

			if(retIndex == -1)
			{
				// No spare slot available.
				_nodes.push_back(node);

				retIndex = _nodes.size() - 1;
			}
		}
	}

	if(retIndex != -1) node -> setHive(GraphHiveHandle(this));

	return retIndex;
}

void GraphHive::removeNode(unsigned nodeIndex)
{
	GraphNode* nodeToDecouple = 0;

	{ SYNC(_lock)

		if(!_active) return;

		if(nodeIndex < _nodes.size() && _nodes[nodeIndex])
		{
			nodeToDecouple = _nodes[nodeIndex];
			_nodes[nodeIndex] = 0;
		}
	}

	if(nodeToDecouple)
	{
		nodeToDecouple -> decouple();
		nodeToDecouple -> decrRef();
	}
}

GraphNodeHandle GraphHive::getNode(std::string nodeName)
{
	GraphNode* foundNode = 0;

	{ SYNC(_lock)

		if(_active)
		{
			for(GraphNode* node : _nodes)
			{
				if(node && node -> getName() == nodeName)
				{
					foundNode = node;
					break;
				}
			}
		}
	}

	return GraphNodeHandle(foundNode);
}

void GraphHive::enumerateThreadPool(unsigned numTabs)
{
	{ SYNC(_lock)

		if(_threadPool) _threadPool -> enumerateState(numTabs);
	}
}
