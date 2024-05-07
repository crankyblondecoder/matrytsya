#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"
#include "GraphNode.hpp"
#include "../thread/ThreadPool.hpp"

extern ThreadPool* threadPool;

GraphAction::~GraphAction()
{
	if(_boundNode) _boundNode -> decrRef();
}

GraphAction::GraphAction()
{
	__edgeTraversalFlags = 0;

	_energy = INITIAL_ENERGY;

	_boundNode = 0;
}

void GraphAction::_setEdgeTraversalFlags(unsigned long flags)
{
	__edgeTraversalFlags = flags;
}

unsigned long GraphAction::getEdgeTraversalFlags()
{
	return __edgeTraversalFlags;
}

void GraphAction::start(GraphNode* origin)
{
	// TODO ... Was the origin ref counted before being passed to this???
	blah;
	_boundNode = origin;

	if(threadPool) {

		// Ask threadpool to execute action work unit.
		threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
	}
	else
	{
		// Action can't be started so this should delete it.
		decrRef();
	}
}

void GraphAction::__work()
{
	if(_boundNode)
	{
		if(_boundNode -> _canActionTarget(this)) _apply(_boundNode);

		if(_energy > 0)
		{
			// TODO ... passing a pointer (this), was it ref incr?
			// TODO ... Shouldn't current bound node be ref decr.
			blah;
			_boundNode = _boundNode -> __traverse(this);

			if(_boundNode)
			{
				// Create and schedule another work unit for the newly bound node. This makes sure
				// actions don't hog thread time.
				threadPool -> executeWorkUnit(new GraphActionThreadPoolWorkUnit(this));
			}
		}
	}

	if(!_boundNode)
	{
		// Action has completed.
		_complete();

		// No more nodes to traverse so allow this action to be deleted.
		decrRef();
	}
}

void GraphAction::__abortWork()
{
	// Action is no longer required.
	// Assume this will make the work units pointer invalid.
	// TODO ... Assumption breaks encapsulation???
	blah;
	decrRef();
}

void GraphAction::__consumeEnergy(int energyAmount)
{
	_energy -= energyAmount;
}