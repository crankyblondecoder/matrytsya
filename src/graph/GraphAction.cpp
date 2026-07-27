#include "../thread/ThreadException.hpp"
#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "../util/Handle.hpp"
#include "GraphHive.hpp"
#include "GraphNode.hpp"

std::atomic<unsigned> GraphAction::_nextId{1};

unsigned GraphAction::_startingEnergy = 512;

GraphAction::~GraphAction()
{
	_boundNode.clear();
}

GraphAction::GraphAction(Handle<GraphNode> initNode, unsigned energy, unsigned numPasses) : _id { _nextId++ },
	_initNode(initNode), _boundNode(initNode), _boundHive(0), _numPasses(numPasses < 1 ? 1 : numPasses)
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
	// A single work cycle for this action: apply to the bound node, then move on to the next one. Only one work
	// unit for this action is ever outstanding, as the next is not submitted until this cycle has finished with
	// the action state.

	bool execWorkUnit = false;
	bool complete = false;

	// Node this action is to be applied to on this work cycle. Invalid if there is nothing to apply to.
	Handle<GraphNode> applyNodeHandle(0);

	{ SYNC(_lock)

		if(_boundNode.isValid())
		{
			GraphNode* curBoundNode = _boundNode.getInstance();

			if(!_initTraverse || _applyToInitNode)
			{
				// Act on currently bound node.
				if(curBoundNode -> getActionable() && curBoundNode -> canActionTarget(this))
				{
					applyNodeHandle = _boundNode;
				}

				// Do any energy accounting.
				// Always consume energy even when action can't target the bound node. This ensures that actions
				// "always die" with certainty. If it was only consumed upon application of a target then potentially
				// an action can get into an infinite loop (this happened in early unit tests).
				__consumeEnergy(curBoundNode -> getEnergyCost());
			}

			_initTraverse = false;
		}
	}

	// Applied outside of the lock because this can re-enter this action, eg via the node emitting an action.
	// Nothing serialises this against other actions, so a node can have several actions applied to it at once
	// and must therefore handle its own internal synchronisation.
	if(applyNodeHandle.isValid()) _apply(applyNodeHandle.getInstance());

	if(__traverse())
	{
		execWorkUnit = true;
	}
	else if(__nextPass())
	{
		// Pass exhausted, but an approved new pass begins from the initial node.
		execWorkUnit = true;
	}
	else
	{
		// Can't traverse and no further passes, which means action is complete.
		complete = true;
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
			Handle<GraphEdge> edgeToTraverse = _boundNode.getInstance() -> traverse(*this);

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

bool GraphAction::_processNextPass(unsigned currentPassNum)
{
	// By default approve continuation; the configured numPasses is the only limit.
	return true;
}

unsigned GraphAction::_getCurrentPassNum()
{
	{ SYNC(_lock)

		return _currentPassNum;
	}
}

bool GraphAction::__nextPass()
{
	unsigned passNum;

	{ SYNC(_lock)

		// A pass has just ended. Do not continue if energy is exhausted or no passes remain.
		// The energy gate also guarantees _boundNode is already empty here, so the reset below
		// does not delete a ref-counted node inside the SYNC block.
		if(_energy == 0 || _currentPassNum >= _numPasses) return false;

		passNum = _currentPassNum;
	}

	// Consult the subclass approval hook outside the lock (external call).
	if(!_processNextPass(passNum)) return false;

	{ SYNC(_lock)

		// Reset traversal state for a fresh complete pass from the initial node.
		// Energy deliberately carries over.
		_currentPassNum++;
		_boundNode = _initNode;
		_initTraverse = true;
		_traversedEdges.clear();
	}

	return true;
}

bool GraphAction::__executeWorkUnit()
{
	bool executed = false;

	Handle<GraphHive> hiveHandle(0);

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

bool GraphAction::canTraverseEdge(Handle<GraphEdge> handle)
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

