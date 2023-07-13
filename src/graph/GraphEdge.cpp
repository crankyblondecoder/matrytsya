#include "GraphEdge.hpp"
#include "GraphNode.hpp"

GraphEdge::~GraphEdge()
{
}

GraphEdge::GraphEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags)
    : _traversalFlags(traversalFlags)
{
	_fromNode = 0;
	_toNode = 0;
	_fromNodeHandle = -1;
	_toNodeHandle = -1;

	_energyCost = 1;

	if(fromNode -> incrRef())
	{
		if(toNode -> incrRef())
		{
			_fromNode = fromNode;
			_toNode = toNode;
		}
		else
		{
			_fromNode = 0;
			fromNode -> decrRef();
		}
	}

	if(_fromNode)
	{
		// Will only get to here if from and to node refs could be obtained.

		_fromNodeHandle = fromNode -> __attachEdge(this);

		if(_fromNodeHandle != -1)
		{
			_toNodeHandle = toNode -> __attachEdge(this);

			if(_toNodeHandle == -1)
			{
				// To node edge attachment failed.
				_fromNode -> __detachEdge(_fromNodeHandle);
				_fromNodeHandle = -1;
			}
		}

		if(_fromNodeHandle == -1)
		{
			// Edge attachment failed.
			_fromNode -> decrRef();
			_toNode -> decrRef();
		}
	}
}

void GraphEdge::__detach()
{
	GraphNode* fromNode = 0;
	GraphNode* toNode = 0;
	unsigned fromNodeHandle;
	unsigned toNodeHandle;

	{ SYNC(_lock)

		if(_fromNode)
		{
			fromNode = _fromNode;
			_fromNode = 0;
			fromNodeHandle = _fromNodeHandle;
			_fromNodeHandle = -1;
		}

		if(_toNode)
		{
			toNode = _toNode;
			_toNode = 0;
			toNodeHandle = _toNodeHandle;
			_toNodeHandle = -1;
		}
	}

	// Don't even think about doing the following inside a sync block!!!

	if(fromNode)
	{
		if(fromNodeHandle != -1) fromNode -> __detachEdge(fromNodeHandle);
		fromNode -> decrRef();
	}

	if(toNode)
	{
		if(toNodeHandle != -1) toNode -> __detachEdge(toNodeHandle);
		toNode -> decrRef();
	}
}

bool GraphEdge::__isComplete()
{
	bool complete;

	{ SYNC(_lock)

		complete = _fromNode && _toNode && _fromNodeHandle != -1 && _toNodeHandle != -1;
	}

	return complete;
}

bool GraphEdge::__canTraverse(GraphNode* origin, GraphAction* action)
{
	// Actions can only traverse edges in the one direction. From the from node to the to node.

	// Check action traversal flags against edge traversal flags.
	return origin == _fromNode && action -> getEdgeTraversalFlags() & _traversalFlags;
}