#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphException.hpp"
#include "GraphNode.hpp"
#include "../thread/ThreadPool.hpp"

extern ThreadPool* threadPool;

GraphAction::~GraphAction()
{
	if(_boundNode) _boundNode -> decrRef();
}

GraphAction::GraphAction(GraphNode* initNode, unsigned energy)
{
	_boundNode = initNode;

	_started = false;
	_stopped = false;

	_energy = energy;
}

void GraphAction::__apply(GraphNode* node)
{
	// Do any energy accounting.
	__consumeEnergy(node -> getActionEnergyCost());

	_apply(node);
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
	return _energy;
}

void GraphAction::__complete()
{
	_stopped = true;

	// Notify subclass.
	_complete();
}

void GraphAction::start()
{
	// This is deliberately not re-entrant!

	if(_started)
	{
		throw new GraphException(GraphException::Error::RE_ENTRY_NOT_PERMITTED);
	}

	if(_stopped)
	{
		throw new GraphException(GraphException::Error::RE_ENTRY_NOT_PERMITTED);
	}

	_started = true;

	bool workSubmitted = false;

	if(threadPool && _boundNode -> incrRef())
	{
		// Bootstrap into action work cycle.
		// Ask threadpool to execute actions work unit.
		workSubmitted = threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
	}

	// Anything beyond this point should just be centered around the work cycle not being able to be established
	// and the associated cleanup. This is because all of the startup variables should have been initialised prior
	// to the work cycle being started.

	if(!workSubmitted)
	{
		if(_boundNode)
		{
			_boundNode -> decrRef();
			_boundNode = 0;
		}

		// Action can't be started so this should delete it.
		decrRef();
	}
}

void GraphAction::work()
{
	// This is an unusually long lock because it is important that any newly scheduled work unit (inside this block)
	// waits at this point for the current work unit to complete. This also means a maximum of two work units can be
	// active for this action at any one time because a new work unit can't be scheduled until the current work unit
	// owns the mutex.

	{ SYNC(_workLock)

		if(_boundNode)
		{
			// Act on currently bound node.
			if(_boundNode -> canActionTarget(this)) _apply(_boundNode);

			if(_energy > 0)
			{
				GraphNode* curBoundNode = _boundNode;

				// Assume that if a node is returned to traverse that it has been ref incr.
				_boundNode = curBoundNode -> traverse(this);

				// A pointer is no longer held for the just traversed node.
				curBoundNode -> decrRef();

				if(_boundNode)
				{
					// Create and schedule another work unit for the newly bound node. This makes sure
					// actions don't hog thread time.

					if(!threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this)))
					{
						// Couldn't schedule work so just trigger action being aborted.
						_boundNode -> decrRef();
						_boundNode = 0;
					}
				}
			}
		}

		// If bound node is non-null at this point then a new work unit was scheduled to process it.

		if(!_boundNode)
		{
			// Action has completed.
			__complete();

			// No more nodes to traverse so allow this action to be deleted.
			decrRef();
		}
	}
}

void GraphAction::abortWork()
{
	// Last scheduled work unit could not be allocated.

	// The bound node has to be decr ref and cleared.
	if(_boundNode) _boundNode -> decrRef();
	_boundNode = 0;

	// Even during abort a subclass must be notified.
	__complete();

	// As this action can no longer traverse, allow this action to be deleted.
	decrRef();
}


