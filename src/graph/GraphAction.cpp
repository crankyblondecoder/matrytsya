#include "../thread/ThreadException.hpp"
#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveHandle.hpp"
#include "GraphNode.hpp"
#include "GraphNodeHandle.hpp"

GraphAction::~GraphAction()
{
	if(_boundNode)
	{
		delete _boundNode;
		_boundNode = 0;
	}
}

GraphAction::GraphAction(GraphNodeHandle& initNode, unsigned energy)
{
	_boundNode = new GraphNodeHandle(initNode);

	_started = false;
	_initTraverse = true;
	_stopped = false;

	_energy = energy;
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

void GraphAction::__apply(GraphNode* node)
{
	// Send to concrete sub-class.
	node -> applyAction(this);
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
	{ SYNC(_workLock)

		return _energy;
	}
}

void GraphAction::__complete()
{
	bool runDecrRef = false;
	bool runCompleteHook = false;

	// This guards the stopped condition.
	_completeCond.lockMutex();

	if(!_stopped)
	{
		_stopped = true;

		runDecrRef = true;
		runCompleteHook = true;

		// Process any threads waiting on condition.
		// Exceptions are allowed to pass through because it is a critical situation to potentially have any threads stalled indefinitely.

		_completeCond.broadcast();
	}

	_completeCond.unlockMutex();

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

	_started = true;

	_completeCond.unlockMutex();

	bool workSubmitted = false;

	if(_boundNode -> isValid())
	{
		GraphHiveHandle hiveHandle = (_boundNode -> getNode()) -> getHive();

		if(hiveHandle.isValid())
		{
			// Bootstrap into action work cycle.
			// Ask threadpool to execute actions work unit.
			workSubmitted = (hiveHandle.getHive()) ->
				executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
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
	bool execWorkUnit = false;
	bool complete = true;

	GraphNode* curBoundNode = 0;

	{ SYNC(_workLock)

		if(_boundNode && _boundNode -> isValid())
		{
			curBoundNode = _boundNode -> getNode();

			if(!_initTraverse)
			{
				// Act on currently bound node.
				if(curBoundNode -> canActionTarget(this)) apply = true;

				// Always consume energy even when action can't target the bound node. This ensures that actions
				// "always die" with certainty. If it was only consumed upon application of a target then potentially
				// an action can get into an infinite loop (this happened in early unit tests).
					// Do any energy accounting.
				__consumeEnergy(curBoundNode -> getEnergyCost());
			}
			else
			{
				// Stops acting on first bound node.
				_initTraverse = false;
			}

			if(_energy > 0)
			{
				GraphNodeHandle* prevBoundNode = _boundNode;

				_boundNode = new GraphNodeHandle(curBoundNode -> traverse());

				delete prevBoundNode;

				if(_boundNode -> isValid())
				{
					execWorkUnit = true;
				}
			}
		}
	}

	if(apply) __apply(curBoundNode);

	if(execWorkUnit)
	{
		// Create and schedule another work unit for the newly bound valid node. This makes sure
		// actions don't hog thread time.
		GraphHiveHandle hiveHandle = (_boundNode -> getNode()) -> getHive();

		if(hiveHandle.isValid())
		{
			if((hiveHandle.getHive()) ->
				executeWorkUnit(new GraphActionThreadPoolWorkUnit(this)))
			{
				// Work successfully scheduled.
				complete = false;
			}
		}
	}

	if(complete)
	{
		// Action has completed.
		__complete();
	}
}

void GraphAction::abortWork()
{
	// Last scheduled work unit could not be allocated. This essentially means the action is stalled so it must
	// complete.

	__complete();
}


