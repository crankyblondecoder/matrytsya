#include "GraphEdge.hpp"
#include "GraphNode.hpp"
#include "GraphNodeHandle.hpp"

std::atomic<unsigned> GraphEdge::_nextId{0};

GraphEdge::~GraphEdge()
{
	if(_toNode) delete _toNode;
}

GraphEdge::GraphEdge(GraphNodeHandle& fromNode) : _id { _nextId++ }
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

unsigned GraphEdge::getId()
{
	return _id;
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
