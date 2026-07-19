#include "../thread/ThreadException.hpp"
#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "GraphHandle.hpp"
#include "GraphHive.hpp"
#include "GraphNode.hpp"

std::atomic<unsigned> GraphAction::_nextId{1};

unsigned GraphAction::_startingEnergy = 65535;

GraphAction::~GraphAction()
{
	_boundNode.clear();
}

GraphAction::GraphAction(GraphHandle<GraphNode> initNode, unsigned energy) : _id { _nextId++ }, _boundNode(initNode), _boundHive(0)
{
	_energy = energy;

	if(initNode.isValid())
	{
		_boundHive = initNode.getInstance() -> getHive();
	}
}

unsigned GraphAction::getId()
{
	return _id;
}

void GraphAction::applyScheduled(GraphHandle<GraphNode> nodeHandle)
{
	{ SYNC(_lock)

		// Sanity check, these two handles should always point to the same node if this function is called.
		if(_boundNode != nodeHandle)
		{
			throw GraphException(GraphException::INVALID_NODE_HANDLE);
		}
	}

	if(nodeHandle.isValid()) _apply(nodeHandle.getInstance());

	if(!(__traverse() && __executeWorkUnit())) __complete();
}

void GraphAction::setApplyToInitialNode()
{
	_applyToInitNode = true;
}

unsigned long GraphAction::getRequiredFlags()
{
	return _requiredFlags;
}

unsigned long GraphAction::getOptionalFlags()
{
	return _optionalFlags;
}

unsigned long GraphAction::getEdgeTraversalFlags()
{
	// For the moment just combine both required and optional flags.
	return _requiredFlags | _optionalFlags;
}

void GraphAction::_addFlag(unsigned long flag, bool required)
{
	if(required)
	{
		_requiredFlags |= flag;
	}
	else
	{
		_optionalFlags |= flag;
	}
}

void GraphAction::waitOnComplete(unsigned timeOut)
{
	// This is required so that this can't be deleted before the condition can be completed.
	if(incrRef())
	{
		try
		{
			_completeCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			decrRef();
			return;
		}

		if(!_stopped)
		{
			try
			{
				if(timeOut > 0)
				{
					unsigned loopLimit = 5;
					unsigned effTimeout = timeOut / loopLimit;
					if(effTimeout < 1) effTimeout = 1;

					while(!_stopped && loopLimit--) _completeCond.waitTimeout(effTimeout);
				}
				else
				{
					while(!_stopped)_completeCond.wait();
				}
			}
			catch(ThreadException& ex)
			{
				_completeCond.unlockMutex();
				decrRef();
				return;
			}
		}

		// Thread is no longer waiting.
		_completeCond.unlockMutex();

		decrRef();
	}
}

bool GraphAction::isComplete()
{
	bool complete;

	_completeCond.lockMutex();

	complete = _stopped;

	_completeCond.unlockMutex();

	return complete;
}

void GraphAction::__consumeEnergy(unsigned amount)
{
	if(amount >= _energy)
	{
		_energy = 0;
	}
	else
	{
		_energy -= amount;
	}
}

unsigned GraphAction::getEnergyLevel()
{
	{ SYNC(_lock)

		return _energy;
	}
}

void GraphAction::setStartingEnergy(unsigned energy)
{
	_startingEnergy = energy;
}

void GraphAction::__complete()
{
	bool runDecrRef = false;
	bool runCompleteHook = false;
	bool runHiveInactive = false;

	// This guards the stopped condition.
	_completeCond.lockMutex();

	if(!_stopped)
	{
		_stopped = true;

		runDecrRef = true;
		runCompleteHook = true;
		runHiveInactive = _hiveActionRegistered;

		_hiveActionRegistered = false;

		// Process any threads waiting on condition.
		// Exceptions are allowed to pass through because it is a critical situation to potentially have any
		// threads stalled indefinitely.

		_completeCond.broadcast();
	}

	_completeCond.unlockMutex();

	// Notify the hive that this action is no longer active.
	if(runHiveInactive && _boundHive.isValid())
	{
		_boundHive.getInstance() -> actionInactive(_hiveActionHandle);
	}

	// Notify subclass.
	if(runCompleteHook) _complete();

	// This allows initial ref count to be released. Must be done last.
	if(runDecrRef) decrRef();
}

void GraphAction::start()
{
	// This is deliberately not re-entrant!

	_completeCond.lockMutex();

	if(_started)
	{
		_completeCond.unlockMutex();
		throw GraphException(GraphException::Error::RE_ENTRY_NOT_PERMITTED);
	}

	if(_stopped)
	{
		_completeCond.unlockMutex();
		throw GraphException(GraphException::Error::RE_ENTRY_NOT_PERMITTED);
	}

	bool workSubmitted = false;

	// Inform any subclass.
	if(_starting())
	{
		_started = true;

		_completeCond.unlockMutex();

		if(_boundNode.isValid())
		{
			if(_boundHive.isValid())
			{
				// Register as active before work is scheduled so that a work unit which completes
				// almost immediately on another thread is guaranteed to see this action as registered.
				_hiveActionHandle = _boundHive.getInstance() -> actionActive(this);
				_hiveActionRegistered = true;

				// Bootstrap into action work cycle.
				// Ask threadpool to execute actions work unit.
				try
				{
					workSubmitted = (_boundHive.getInstance()) ->
						executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
				}
				catch(std::bad_alloc& ex)
				{
					workSubmitted = false;
				}
			}
		}
	}

	// Anything beyond this point should just be centered around the work cycle not being able to be established
	// and the associated cleanup. This is because all of the startup variables should have been initialised prior
	// to the work cycle being started.

	if(!workSubmitted)
	{
		// Action can't be started so is considered complete.
		__complete();
	}
}

void GraphAction::work()
{
	// This is an unusually long lock because it is important that any newly scheduled work unit (inside this block)
	// waits at this point for the current work unit to complete. This also means a maximum of two work units can be
	// active for this action at any one time because a new work unit can't be scheduled until the current work unit
	// owns the mutex.

	bool apply = false;
	bool traverse = false;
	bool execWorkUnit = false;
	bool complete = false;

	GraphNode* curBoundNode = 0;
	GraphHandle<GraphNode> prevBoundNodeHandle(0);

	{ SYNC(_lock)

		if(_boundNode.isValid())
		{
			curBoundNode = _boundNode.getInstance();

			if(!_initTraverse || _applyToInitNode)
			{
				// Act on currently bound node.
				if(curBoundNode -> canActionTarget(this)) apply = true;

				// Do any energy accounting.
				// Always consume energy even when action can't target the bound node. This ensures that actions
				// "always die" with certainty. If it was only consumed upon application of a target then potentially
				// an action can get into an infinite loop (this happened in early unit tests).
				__consumeEnergy(curBoundNode -> getEnergyCost());
			}

			_initTraverse = false;
		}
	}

	if(apply)
	{
		// Note: Successfully scheduling an action means traversal, work unit execution and action completion
		// are processed later on.

		if(!_boundNode.getInstance() -> scheduleAction(GraphHandle<GraphAction>(this)))
		{
			// Couldn't schedule this action with the node so just try and keep action traversal going.
			traverse = true;
		}
	}
	else
	{
		// Still should attempt to traverse, even if not applied.
		traverse = true;
	}

	if(traverse)
	{
		if(__traverse())
		{
			execWorkUnit = true;
		}
		else
		{
			// Can't traverse, which means action is complete.
			execWorkUnit = false;
			complete = true;
		}
	}

	if(execWorkUnit)
	{
		// Being unable to execute a work unit forces an action to be complete.
		complete = !__executeWorkUnit();
	}

	if(complete)
	{
		// Action has completed.
		__complete();
	}
}

bool GraphAction::__traverse()
{
	bool traversed = false;

	{ SYNC(_lock)

		if(_energy > 0 && _boundNode.isValid())
		{
			GraphHandle<GraphEdge> edgeToTraverse = _boundNode.getInstance() -> traverse(*this);

			if(edgeToTraverse.isValid())
			{
				_traversedEdges.push_back(edgeToTraverse.getInstance() -> getId());

				_boundNode = edgeToTraverse.getInstance() -> traverse();

				traversed = _boundNode.isValid();
			}
			else
			{
				_boundNode.clear();
			}
		}
	}

	return traversed;
}

bool GraphAction::__executeWorkUnit()
{
	bool executed = false;

	GraphHandle<GraphHive> hiveHandle(0);

	{ SYNC(_lock)

		if(_boundNode.isValid())
		{
			hiveHandle = _boundNode.getInstance() -> getHive();
		}
	}

	if(hiveHandle.isValid())
	{
		// Create and schedule another work unit for the currently bound valid node.

		try
		{
			executed = (hiveHandle.getInstance()) ->
				executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
		}
		catch(std::bad_alloc& ex)
		{
			executed = false;
		}
	}

	return executed;
}

void GraphAction::abortWork()
{
	// Last scheduled work unit could not be allocated. This essentially means the action is stalled so it must
	// complete.

	__complete();
}

bool GraphAction::canTraverseEdge(GraphHandle<GraphEdge> handle)
{
	bool canTraverse = true;

	// Standard behaviour is to not traverse an edge that has already been traversed.
	// This was introduced to support required Transform Node behaviour.

	if(handle.isValid())
	{
		unsigned edgeId = handle.getInstance() -> getId();

		unsigned numTraversedEdges = _traversedEdges.size();

		for(unsigned index = 0; index < numTraversedEdges; index++)
		{
			if(_traversedEdges[index] == edgeId)
			{
				// Edge has already been traversed by this action.
				canTraverse = false;
				break;
			}
		}
	}

	return canTraverse;
}

