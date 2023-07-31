#include "GraphAction.hpp"
#include "GraphNode.hpp"

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

	// TODO Request to start a thread somehow ...
}
