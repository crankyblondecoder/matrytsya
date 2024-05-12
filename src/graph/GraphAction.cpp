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

GraphAction::GraphAction() : _started{0}, _boundNode{0}, __edgeTraversalFlags{0}
{
	_energy = INITIAL_ENERGY;
}

void GraphAction::_setEdgeTraversalFlags(unsigned long flags)
{
	__edgeTraversalFlags = flags;
}

unsigned long GraphAction::getEdgeTraversalFlags()
{
	return __edgeTraversalFlags;
}

void GraphAction::__start(GraphNode* origin)
{
	// This is deliberately not re-entrant!

	if(_started)
	{
		throw new GraphException(GraphException::Error::RE_ENTRY_NOT_PERMITTED);
	}

	_started = true;

	bool workSubmitted = false;

	if(origin -> incrRef())
	{
		_boundNode = origin;

		if(threadPool) {

			// Ask threadpool to execute action work unit.
			workSubmitted = threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
		}
	}

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

void GraphAction::__work()
{
	// This is an unusually long lock because it is important that any newly scheduled work unit waits at this point for the
	// current work unit to complete. This also means a maximum of two work units can be active for this action at any one
	// time because a new work unit can't be scheduled until the current work unit owns the mutex.

	{ SYNC(_workLock)

		if(_boundNode)
		{
			if(_boundNode -> _canActionTarget(this)) _apply(_boundNode);

			if(_energy > 0)
			{
				GraphNode* curBoundNode = _boundNode;

				// Assume that if a node is returned to traverse that it has been ref incr.
				_boundNode = curBoundNode -> __traverse(this);

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
			_complete();

			// No more nodes to traverse so allow this action to be deleted.
			decrRef();
		}
	}
}

void GraphAction::__abortWork()
{
	// Last scheduled work unit could not be allocated.

	// The bound node has to be decr ref and cleared.
	if(_boundNode) _boundNode -> decrRef();
	_boundNode = 0;

	// Even during abort a subclass must be notified.
	_complete();

	// As this action can no longer traverse, allow this action to be deleted.
	decrRef();
}

void GraphAction::__consumeEnergy(int energyAmount)
{
	_energy -= energyAmount;
}