#include "GraphAction.hpp"
#include "GraphNode.hpp"
#include "../thread/ThreadPool.hpp"
#include "../thread/ThreadPoolWorkUnit.hpp"

extern ThreadPool* threadPool;

GraphAction::~GraphAction()
{
	if(__boundNode) {

		__boundNode -> wontTraverse();
	}
}

GraphAction::GraphAction()
{
	__edgeTraversalFlags = 0;

	__energy = 255;

	__boundNode = 0;
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
	__boundNode = origin;

	if(incrRef()) {

		// TODO Request to start a thread somehow ...
		blah

		// Make sure work unit is deleted if it could not be executed!!! ???
	}
	else
	{
		// Action can't be started so this should delete it.
		decrRef();
	}
}

void GraphAction::work()
{
	// TODO make sure this action can still be applied to the node. There is no point is going any further if it can't ...
	blah

	// TODO start work ...
	blah

	// Actions are designed to be "one shots" in terms of threads assigned to them. So decrRef which should cause this
	// to be deleted. It would be abnormal behaviour if it didn't.

	decrRef();
}

void GraphAction::abortWork()
{
	decrRef();
}