#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphException.hpp"
#include "GraphNode.hpp"
#include "GraphNodeHandle.hpp"
#include "../thread/ThreadPool.hpp"

extern ThreadPool* threadPool;

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

void GraphAction::__apply(GraphNode* node)
{
	// Do any energy accounting.
	__consumeEnergy(node -> getEnergyCost());

	// Send to concrete sub-class.
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

	// This allows initial ref count to be released.
	decrRef();
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

	if(threadPool && _boundNode -> isValid())
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

		bool complete = true;

		if(_boundNode && _boundNode -> isValid())
		{
			GraphNode* curBoundNode = _boundNode -> getNode();

			if(!_initTraverse)
			{
				// Act on currently bound node.
				if(curBoundNode -> canActionTarget(this)) __apply(curBoundNode);
			}
			else
			{
				// Don't act on first bound node.
				_initTraverse = false;
			}

			if(_energy > 0)
			{
				delete _boundNode;

				_boundNode = new GraphNodeHandle(curBoundNode -> traverse());

				if(_boundNode -> isValid())
				{
					// Create and schedule another work unit for the newly bound valid node. This makes sure
					// actions don't hog thread time.

					if(threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this)))
					{
						// Work successfully scheduled.
						complete = false;
					}
				}
			}
		}

		if(complete)
		{
			// Action has completed.
			__complete();
		}
	}
}

void GraphAction::abortWork()
{
	// Last scheduled work unit could not be allocated. This essentially means the action is stalled so it must
	// complete.

	__complete();
}


