#include <stdexcept>

#include "../thread/ThreadException.hpp"
#include "../thread/ThreadPool.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveStrobeScheduler.hpp"
#include "GraphNode.hpp"

GraphHive::~GraphHive()
{
	// shutdown() (if it ran) already stopped the scheduler thread; only the actual deallocation is
	// deferred to here. Since this destructor only runs once nothing holds a reference to this hive,
	// nothing can be concurrently inside setStrobeEmitter()/clearStrobeEmitter()/removeNode() using
	// _strobeScheduler, so it's safe to delete without any additional locking.
	if(_strobeScheduler)
	{
		_strobeScheduler -> stop(true);
		delete _strobeScheduler;
		_strobeScheduler = 0;
	}
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
			delete _threadPool;
			_threadPool = 0;

			std::string msg = "Critical error: thread pool did not become active.";
			throw std::runtime_error(msg);
		}

		// Dedicated thread that drives per-node strobe emission.
		_strobeScheduler = new GraphHiveStrobeScheduler();
		_strobeScheduler -> start();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: hive threading creation failed -> " +
			ex.getSubsystemErrorString();

		if(_strobeScheduler)
		{
			_strobeScheduler -> stop(true);
			delete _strobeScheduler;
			_strobeScheduler = 0;
		}

		if(_threadPool)
		{
			_threadPool -> shutdown();
			delete _threadPool;
			_threadPool = 0;
		}

		throw std::runtime_error(msg);
	}

	_active = true;
}

GraphHandle<GraphNode> GraphHive::__findNode(unsigned nodeId)
{
	{ SYNC(_lock)

		for(GraphNode* nodePtr : _nodes)
		{
			if(nodePtr && nodePtr -> getId() == nodeId)
			{
				return GraphHandle<GraphNode>(nodePtr);
			}
		}
	}

	return GraphHandle<GraphNode>(0);
}

void GraphHive::poke(unsigned nodeId, GraphPoke poke)
{
	GraphHandle<GraphNode> found = __findNode(nodeId);

	if(found.isValid()) found.getInstance() -> poke(poke);
}

void GraphHive::setStrobeEmitter(GraphHandle<StrobeEmitterNode> nodeHandle, unsigned frequencyHz)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	_strobeScheduler -> setEmitter(nodeHandle, frequencyHz);
}

void GraphHive::clearStrobeEmitter(GraphHandle<StrobeEmitterNode> nodeHandle)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	if(nodeHandle.isValid()) _strobeScheduler -> removeEmitter(nodeHandle.getInstance());
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

	// Update current action active count and signal anything waiting on it to change.

	_activeActionCountCond.lockMutex();

	if(_activeActionCount > -1)
	{
		_activeActionCount++;

		if(!_initialActionActive)
		{
			_initialActionActive = true;

			// Wake up anything waiting on this condition for the initial action to become active.
			_activeActionCountCond.broadcast();
		}
	}

	_activeActionCountCond.unlockMutex();

	// Update accumulated active action count and signal anything waiting on it to change.

	_activeActionCountAccumCond.lockMutex();

	if(_activeActionCountAccum > -1)
	{
		_activeActionCountAccum++;

		_activeActionCountAccumCond.broadcast();
	}

	_activeActionCountAccumCond.unlockMutex();

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
		_activeActionCountCond.lockMutex();

		if(_activeActionCount > -1)
		{
			_activeActionCount--;

			if(_activeActionCount == 0) _activeActionCountCond.broadcast();
		}

		_activeActionCountCond.unlockMutex();
	}
}

void GraphHive::waitOnNoActionsActive(unsigned timeOut)
{
	// This is required so that this can't be deleted before the condition can be completed.
	if(incrRef())
	{
		try
		{
			_activeActionCountCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			decrRef();
			return;
		}

		// Must take into account shutdown indicator value.
		if(_activeActionCount > 0)
		{
			try
			{
				if(timeOut > 0)
				{
					unsigned loopLimit = 5;
					unsigned effTimeout = timeOut / loopLimit;
					if(effTimeout < 1) effTimeout = 1;

					while(_activeActionCount > 0 && loopLimit--) _activeActionCountCond.waitTimeout(effTimeout);
				}
				else
				{
					while(_activeActionCount > 0) _activeActionCountCond.wait();
				}
			}
			catch(ThreadException& ex)
			{
				_activeActionCountCond.unlockMutex();
				decrRef();
				return;
			}
		}

		// Thread is no longer waiting.
		_activeActionCountCond.unlockMutex();

		decrRef();
	}
}

void GraphHive::waitOnInitialActionActive(unsigned timeOut)
{
	// This is required so that this can't be deleted before the condition can be completed.
	if(incrRef())
	{
		try
		{
			_activeActionCountCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			decrRef();
			return;
		}

		if(!_initialActionActive && _activeActionCount > -1)
		{
			try
			{
				if(timeOut > 0)
				{
					unsigned loopLimit = 5;
					unsigned effTimeout = timeOut / loopLimit;
					if(effTimeout < 1) effTimeout = 1;

					while(!_initialActionActive && _activeActionCount > -1 && loopLimit--) _activeActionCountCond.waitTimeout(effTimeout);
				}
				else
				{
					while(!_initialActionActive && _activeActionCount > -1) _activeActionCountCond.wait();
				}
			}
			catch(ThreadException& ex)
			{
				_activeActionCountCond.unlockMutex();
				decrRef();
				return;
			}
		}

		// Thread is no longer waiting.
		_activeActionCountCond.unlockMutex();

		decrRef();
	}
}

void GraphHive::waitOnActionActiveCountAccum(int count, unsigned timeOut)
{
	// This is required so that this can't be deleted before the condition can be completed.
	if(incrRef())
	{
		try
		{
			_activeActionCountAccumCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			decrRef();
			return;
		}

		if(_activeActionCountAccum > -1 && _activeActionCountAccum < count)
		{
			try
			{
				if(timeOut > 0)
				{
					unsigned loopLimit = 5;
					unsigned effTimeout = timeOut / loopLimit;
					if(effTimeout < 1) effTimeout = 1;

					while(_activeActionCountAccum > -1 && _activeActionCountAccum < count && loopLimit--)
						_activeActionCountAccumCond.waitTimeout(effTimeout);
				}
				else
				{
					while(_activeActionCountAccum > -1 && _activeActionCountAccum < count)
						_activeActionCountAccumCond.wait();
				}
			}
			catch(ThreadException& ex)
			{
				_activeActionCountAccumCond.unlockMutex();
				decrRef();
				return;
			}
		}

		// Thread is no longer waiting.
		_activeActionCountAccumCond.unlockMutex();

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

	// TODO ... There is a potential null pointer situation here if the hive is shutdown at this point.

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

		// TODO ... This shouldn't be called in a SYNC block but for the moment can be assumed to not cause re-entry
		// on this thread.

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

	// Stop strobe emission before tearing anything down: emitStrobe() schedules work onto the
	// thread pool, so the scheduler thread must not be running when the pool and nodes go away.
	// The scheduler object itself is only actually deleted in ~GraphHive(), once nothing can still
	// be referencing this hive to reach it.
	if(_strobeScheduler) _strobeScheduler -> stop(true);

	// Required so that active count wait can terminate.
	try
	{
		_activeActionCountCond.lockMutex();
		_activeActionCount = -1;
		_activeActionCountCond.broadcast();
		_activeActionCountCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		// Just continue on with trying to shutdown.
	}

	try
	{
		_activeActionCountAccumCond.lockMutex();
		_activeActionCountAccum = -1;
		_activeActionCountAccumCond.broadcast();
		_activeActionCountAccumCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		// Just continue on with trying to shutdown.
	}

	for(unsigned index = 0; index < _nodes.size(); index++)
	{
		GraphNode* node = _nodes[index];

		if(node)
		{
			node -> decouple();
			node -> decrRef();

			_nodes[index] = 0;
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

void GraphHive::addNode(GraphNode* node)
{
	// Increment the ref to the node so that shutdown can't delete it pre-maturely.
	if(!node -> incrRef()) return;

	int addedIndex = -1;
	bool removeInitRef = false;

	{ SYNC(_lock)

		if(_active)
		{
			for(unsigned index = 0; index < _nodes.size(); index++)
			{
				if(!_nodes[index])
				{
					_nodes[index] = node;
					addedIndex = index;
					break;
				}
			}

			if(addedIndex == -1)
			{
				// No spare slot available.
				_nodes.push_back(node);

				addedIndex = _nodes.size() - 1;
			}
		}
		else
		{
			removeInitRef = true;
		}
	}

	// Note: It is possible for shutdown to be invoked here on another thread.

	if(addedIndex != -1)
	{
		// Rely on the node rejecting being added to the hive if it has been decoupled. This is possibly a bit brittle.
		bool accepted = node -> setHive(GraphHandle<GraphHive>(this));

		if(!accepted)
		{
			// If not already, remove from nodes vector and remove initial ref.

			{ SYNC(_lock)

				if(_active)
				{
					_nodes[addedIndex] = 0;
					removeInitRef = true;
				}
			}

			addedIndex = -1;
		}
	}

	if(removeInitRef) node -> decrRef();

	// Remove the ref added at the beginning of this function.
	node -> decrRef();
}

void GraphHive::removeNode(GraphHandle<GraphNode> nodeHandle)
{
	if(!nodeHandle.isValid()) return;

	GraphNode* nodeToFind = nodeHandle.getInstance();
	bool decouple = false;

	{ SYNC(_lock)

		if(!_active) return;

		for(unsigned index = 0; index < _nodes.size(); index++)
		{
			if(_nodes[index] == nodeToFind)
			{
				decouple = true;
				_nodes[index] = 0;
				break;
			}
		}
	}

	if(decouple)
	{
		// A node that is removed from the hive must also stop being a strobe emitter.
		if(_strobeScheduler) _strobeScheduler -> removeEmitter(nodeToFind);

		nodeToFind -> decouple();
		nodeToFind -> decrRef();
	}
}

GraphHandle<GraphNode> GraphHive::getNode(std::string nodeName)
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

	return GraphHandle<GraphNode>(foundNode);
}

void GraphHive::enumerateThreadPool(unsigned numTabs)
{
	// Note: this function is assumed to be called as a diagnostic and so the SYNC rules can be relaxed.

	{ SYNC(_lock)

		if(_active && _threadPool) _threadPool -> enumerateState(numTabs);
	}
}
