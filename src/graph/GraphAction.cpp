#include "GraphAction.hpp"

GraphAction::~GraphAction()
{
}

GraphAction::GraphAction()
{
	_edgeTraversalFlags = 0;
}

void GraphAction::setEdgeTraversalFlags(unsigned long flags)
{
	_edgeTraversalFlags = flags;
}

unsigned long GraphAction::getEdgeTraversalFlags()
{
	return _edgeTraversalFlags;
}