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

		_fromNodeHandle = fromNode -> attachEdge(this);

		if(_fromNodeHandle != -1)
		{
			_toNodeHandle = toNode -> attachEdge(this);

			if(_toNodeHandle == -1)
			{
				// To node edge attachment failed.
				_fromNode -> detachEdge(_fromNodeHandle);
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

void GraphEdge::detach()
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
		if(fromNodeHandle != -1) fromNode -> detachEdge(fromNodeHandle);
		fromNode -> decrRef();
	}

	if(toNode)
	{
		if(toNodeHandle != -1) toNode -> detachEdge(toNodeHandle);
		toNode -> decrRef();
	}
}

bool GraphEdge::isComplete()
{
	bool complete;

	{ SYNC(_lock)

		complete = _fromNode && _toNode && _fromNodeHandle != -1 && _toNodeHandle != -1;
	}

	return complete;
}

bool GraphEdge::canTraverse(GraphAction* action)
{
	// Check action traversal flags against edge traversal flags.
	return action -> getEdgeTraversalFlags() & _traversalFlags;
}