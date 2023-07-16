#include "GraphAction.hpp"

GraphAction::~GraphAction()
{
}

GraphAction::GraphAction()
{
	__edgeTraversalFlags = 0;

	__energy = 255;
}

void GraphAction::_setEdgeTraversalFlags(unsigned long flags)
{
	__edgeTraversalFlags = flags;
}

unsigned long GraphAction::getEdgeTraversalFlags()
{
	return __edgeTraversalFlags;
}