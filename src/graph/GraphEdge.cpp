#include "GraphEdge.hpp"
#include "GraphHandle.hpp"
#include "GraphNode.hpp"

std::atomic<unsigned> GraphEdge::_nextId{0};

GraphEdge::~GraphEdge()
{
	if(_toNode) delete _toNode;
}

GraphEdge::GraphEdge(GraphHandle<GraphNode>& fromNode) : _id { _nextId++ }
{
	_toNode = 0;

	// Try and make a copy of the handle.
	GraphHandle<GraphNode>* newHandle = new GraphHandle<GraphNode>(fromNode);

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

GraphHandle<GraphNode> GraphEdge::traverse()
{
	if(_toNode)
	{
		GraphHandle<GraphNode> retHandle(*_toNode);
		return retHandle;
	}

	return 0;
}
