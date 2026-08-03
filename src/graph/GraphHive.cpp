#include <stdexcept>

#include "../agent/AgenticHarness.hpp"
#include "../agent/ModelContext.hpp"
#include "../thread/ThreadException.hpp"
#include "../thread/ThreadPool.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveSceneSurface.hpp"
#include "GraphHiveStrobeScheduler.hpp"
#include "GraphHiveSurface.hpp"
#include "GraphNode.hpp"

GraphHive::~GraphHive()
{
	// shutdown() (if it ran) already stopped the scheduler thread; only the actual deallocation is
	// deferred to here. Since this destructor only runs once nothing holds a reference to this hive,
	// nothing can be concurrently inside setStrobeEmitter()/clearStrobeEmitter()/removeNode()/
	// setStrobeSurface()/clearStrobeSurface()/removeSurface() using _strobeScheduler, so it's safe to
	// delete without any additional locking.
	if(_strobeScheduler)
	{
		_strobeScheduler -> stop(true);
		delete _strobeScheduler;
		_strobeScheduler = 0;
	}
}

GraphHive::GraphHive(unsigned numThreads) : _agenticHarness(0), _toolBindingsFactory(0)
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

Handle<GraphNode> GraphHive::__findNode(unsigned nodeId)
{
	{ SYNC(_lock)

		for(GraphNode* nodePtr : _nodes)
		{
			if(nodePtr && nodePtr -> getId() == nodeId)
			{
				return Handle<GraphNode>(nodePtr);
			}
		}
	}

	return Handle<GraphNode>(0);
}

void GraphHive::poke(unsigned nodeId, GraphPoke poke)
{
	Handle<GraphNode> found = __findNode(nodeId);

	if(found.isValid()) found.getInstance() -> poke(poke);
}

void GraphHive::setStrobeEmitter(Handle<StrobeEmitterNode> nodeHandle, unsigned periodMs)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	_strobeScheduler -> setEmitter(nodeHandle, periodMs);
}

void GraphHive::clearStrobeEmitter(Handle<StrobeEmitterNode> nodeHandle)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	if(nodeHandle.isValid()) _strobeScheduler -> removeEmitter(nodeHandle);
}

void GraphHive::setStrobeSurface(Handle<GraphHiveSurface> surfaceHandle, unsigned periodMs)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	_strobeScheduler -> setSurface(surfaceHandle, periodMs);
}

void GraphHive::clearStrobeSurface(Handle<GraphHiveSurface> surfaceHandle)
{
	{ SYNC(_lock)

		if(!_active) return;
	}

	if(surfaceHandle.isValid()) _strobeScheduler -> removeSurface(surfaceHandle);
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

void GraphHive::setAgenticHarness(Handle<AgenticHarness> agenticHarness)
{
	{ SYNC(_lock)

		if(_active) _agenticHarness = agenticHarness;
	}
}

Handle<AgenticHarness> GraphHive::getAgenticHarness()
{
	{ SYNC(_lock)

		return _agenticHarness;
	}
}

void GraphHive::setToolBindingsFactory(Handle<GraphToolBindingsFactory> toolBindingsFactory)
{
	{ SYNC(_lock)

		if(_active) _toolBindingsFactory = toolBindingsFactory;
	}
}

Handle<GraphToolBindingsFactory> GraphHive::getToolBindingsFactory()
{
	{ SYNC(_lock)

		return _toolBindingsFactory;
	}
}

Handle<ModelContext> GraphHive::createModelContext(AgenticHarness::Role role, AgenticHarness::Capability capability)
{
	Handle<AgenticHarness> harness(0);

	{ SYNC(_lock)

		harness = _agenticHarness;
	}

	if(!harness.isValid()) throw GraphException(GraphException::AGENTIC_HARNESS_NOT_SET);

	return harness.getInstance() -> createContext(role, capability);
}

Handle<ModelContext> GraphHive::processAgenticRequest(AgenticHarness::Capability capability, std::string prompt,
	Handle<ModelContext> context)
{
	return __processAgenticRequest(AgenticHarness::Role::CHAT, capability, prompt, context);
}

Handle<ModelContext> GraphHive::processNodeAgenticRequest(AgenticHarness::Capability capability, std::string prompt,
	Handle<ModelContext> context)
{
	return __processAgenticRequest(AgenticHarness::Role::NODE, capability, prompt, context);
}

Handle<ModelContext> GraphHive::__processAgenticRequest(AgenticHarness::Role role, AgenticHarness::Capability capability,
	std::string prompt, Handle<ModelContext> context)
{
	Handle<AgenticHarness> harness(0);

	{ SYNC(_lock)

		harness = _agenticHarness;
	}

	if(!harness.isValid()) throw GraphException(GraphException::AGENTIC_HARNESS_NOT_SET);

	return harness.getInstance() -> processRequest(prompt, role, capability, context);
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
	if(_strobeScheduler)
	{
		_strobeScheduler -> stop(true);

		// The scheduler thread is stopped, so nothing can still be iterating _entries/_surfaceEntries;
		// release the held handles now rather than waiting for ~GraphHive to delete the scheduler.
		_strobeScheduler -> clearEmitters();
		_strobeScheduler -> clearSurfaces();
	}

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

	for(unsigned index = 0; index < _surfaces.size(); index++)
	{
		GraphHiveSurface* surface = _surfaces[index];

		if(surface)
		{
			// close() handles both the subclass cleanup hook and the decrRef of the hive-owned reference.
			surface -> close();

			_surfaces[index] = 0;
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

			_bumpVersion();
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
		bool accepted = node -> setHive(Handle<GraphHive>(this));

		if(!accepted)
		{
			// If not already, remove from nodes vector and remove initial ref.

			{ SYNC(_lock)

				if(_active)
				{
					_nodes[addedIndex] = 0;
					removeInitRef = true;

					_bumpVersion();
				}
			}

			addedIndex = -1;
		}
	}

	if(removeInitRef) node -> decrRef();

	// Remove the ref added at the beginning of this function.
	node -> decrRef();
}

void GraphHive::removeNode(Handle<GraphNode> nodeHandle)
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

				_bumpVersion();

				break;
			}
		}
	}

	if(decouple)
	{
		// A node that is removed from the hive must also stop being a strobe emitter. Only nodes that
		// are actually StrobeEmitterNodes can ever be registered, so a failed cast here just means
		// the node was never an emitter.
		if(_strobeScheduler) _strobeScheduler -> removeEmitter(Handle<StrobeEmitterNode>(dynamic_cast<StrobeEmitterNode*>(nodeToFind)));

		nodeToFind -> decouple();
		nodeToFind -> decrRef();
	}
}

Handle<GraphNode> GraphHive::getNode(std::string nodeName)
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

	return Handle<GraphNode>(foundNode);
}

std::vector<std::string> GraphHive::getNodeNames()
{
	std::vector<std::string> names;

	{ SYNC(_lock)

		if(_active)
		{
			for(GraphNode* node : _nodes)
			{
				if(node) names.push_back(node -> getName());
			}
		}
	}

	return names;
}

void GraphHive::addSurface(GraphHiveSurface* surface)
{
	// Increment the ref to the surface so that shutdown can't delete it pre-maturely.
	if(!surface -> incrRef()) return;

	bool added = false;

	{ SYNC(_lock)

		if(_active)
		{
			for(unsigned index = 0; index < _surfaces.size(); index++)
			{
				if(!_surfaces[index])
				{
					_surfaces[index] = surface;
					added = true;
					break;
				}
			}

			if(!added)
			{
				// No spare slot available.
				_surfaces.push_back(surface);

				added = true;
			}

			_bumpVersion();
		}
	}

	if(added)
	{
		surface -> setHive(Handle<GraphHive>(this));
	}
	else
	{
		// If not added then because this hive manages the initial ref of the surface, decr-ref it.
		surface -> decrRef();
	}

	// Remove the ref added at the beginning of this function.
	surface -> decrRef();
}

void GraphHive::removeSurface(Handle<GraphHiveSurface> surfaceHandle)
{
	if(!surfaceHandle.isValid()) return;

	GraphHiveSurface* surfaceToFind = surfaceHandle.getInstance();
	bool removed = false;

	{ SYNC(_lock)

		if(!_active) return;

		for(unsigned index = 0; index < _surfaces.size(); index++)
		{
			if(_surfaces[index] == surfaceToFind)
			{
				removed = true;
				_surfaces[index] = 0;

				_bumpVersion();

				break;
			}
		}
	}

	if(removed)
	{
		// A surface that is removed from the hive must also stop being strobed.
		if(_strobeScheduler) _strobeScheduler -> removeSurface(surfaceHandle);

		// close() handles both the subclass cleanup hook and the decrRef of the hive-owned reference.
		surfaceToFind -> close();
	}
}

Handle<GraphHiveSurface> GraphHive::getSurface(std::string surfaceName)
{
	GraphHiveSurface* foundSurface = 0;

	{ SYNC(_lock)

		if(_active)
		{
			for(GraphHiveSurface* surface : _surfaces)
			{
				if(surface && surface -> getName() == surfaceName)
				{
					foundSurface = surface;
					break;
				}
			}
		}
	}

	return Handle<GraphHiveSurface>(foundSurface);
}

std::vector<std::string> GraphHive::getSurfaceNames()
{
	std::vector<std::string> names;

	{ SYNC(_lock)

		if(_active)
		{
			for(GraphHiveSurface* surface : _surfaces)
			{
				if(surface) names.push_back(surface -> getName());
			}
		}
	}

	return names;
}

Handle<GraphHiveSceneSurface> GraphHive::getSceneSurface(std::string surfaceName)
{
	Handle<GraphHiveSurface> surface = getSurface(surfaceName);

	if(surface.isValid() && surface.getInstance() -> getType() == GraphHiveSurface::Type::SCENE_SURFACE)
	{
		return Handle<GraphHiveSceneSurface>(static_cast<GraphHiveSceneSurface*>(surface.getInstance()));
	}

	return Handle<GraphHiveSceneSurface>(0);
}

Handle<GraphHiveSceneSurface> GraphHive::getDefaultSceneSurface()
{
	GraphHiveSurface* foundSurface = 0;

	{ SYNC(_lock)

		if(_active)
		{
			for(GraphHiveSurface* surface : _surfaces)
			{
				if(surface && surface -> getType() == GraphHiveSurface::Type::SCENE_SURFACE && surface -> getDefault())
				{
					foundSurface = surface;
					break;
				}
			}
		}
	}

	return Handle<GraphHiveSceneSurface>(static_cast<GraphHiveSceneSurface*>(foundSurface));
}

void GraphHive::enumerateThreadPool(unsigned numTabs)
{
	// Note: this function is assumed to be called as a diagnostic and so the SYNC rules can be relaxed.

	{ SYNC(_lock)

		if(_active && _threadPool) _threadPool -> enumerateState(numTabs);
	}
}
