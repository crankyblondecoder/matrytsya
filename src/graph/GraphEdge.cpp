#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "GraphNodeHandle.hpp"

GraphEdge::~GraphEdge()
{
	if(_toNode) delete _toNode;
}

GraphEdge::GraphEdge(GraphNodeHandle& fromNode)
{
	_toNode = 0;

	// Try and make a copy of the handle.
	GraphNodeHandle* newHandle = new GraphNodeHandle(fromNode);

	if(newHandle -> isValid())
	{
		_toNode = newHandle;
	}
	else
	{
		delete newHandle;
	}
}

void GraphEdge::boundToNode()
{
	if(_toNode) (_toNode -> getNode()) -> referredTo(this);
}

bool GraphEdge::isComplete()
{
	return _toNode != 0;
}

GraphNodeHandle GraphEdge::traverse()
{
	if(_toNode)
	{
		GraphNodeHandle retHandle(*_toNode);
		return retHandle;
	}

	return 0;
}
