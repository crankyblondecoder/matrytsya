#include "GraphEdge.hpp"
#include "GraphNode.hpp"

GraphEdge::~GraphEdge()
{
	// Because this is ref counted it shouldn't be possible to get here without being fully detached.
}

GraphEdge::GraphEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags)
    : _traversalFlags(traversalFlags)
{
	_fromNode = 0;
	_toNode = 0;
	_fromNodeHandle = -1;
	_toNodeHandle = -1;

	_energyCost = DEFAULT_ENERGY_COST;

	// Attempt to ref count the nodes so that their pointers can be stored.

	if(fromNode -> incrRef())
	{
		if(toNode -> incrRef())
		{
			_fromNode = fromNode;
			_toNode = toNode;
		}
		else
		{
			fromNode -> decrRef();
		}
	}

	if(_fromNode)
	{
		// Will only get to here if from and to node refs could be obtained.

		_fromNodeHandle = fromNode -> __addEdge(this);

		if(_fromNodeHandle != -1)
		{
			_toNodeHandle = toNode -> __addEdge(this);

			if(_toNodeHandle == -1)
			{
				// To node edge attachment failed.
				_fromNode -> __removeEdge(_fromNodeHandle);
				_fromNodeHandle = -1;
			}
		}

		if(_fromNodeHandle == -1)
		{
			// Edge attachment failed.

			_fromNode -> decrRef();
			_toNode -> decrRef();

			_fromNode = 0;
			_toNode = 0;
		}
	}
}

void GraphEdge::__detach()
{
	GraphNode* fromNode = 0;
	GraphNode* toNode = 0;
	int fromNodeHandle;
	int toNodeHandle;

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
		if(fromNodeHandle != -1) fromNode -> __removeEdge(fromNodeHandle);
		fromNode -> decrRef();
	}

	if(toNode)
	{
		if(toNodeHandle != -1) toNode -> __removeEdge(toNodeHandle);
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
	bool retVal;

	// Get this here so a function isn't called in a sync block.
	unsigned edgeTravFlags = action -> getEdgeTraversalFlags();

	// Actions can only traverse edges in the one direction. From the from node to the to node.

	{ SYNC(_lock)

		// Check action traversal flags against edge traversal flags.
		retVal = _fromNode && _toNode && (origin == _fromNode) && (edgeTravFlags & _traversalFlags);
	}

	return retVal;
}

GraphNode* GraphEdge::__traverse(GraphNode* origin, GraphAction* action)
{
	GraphNode* retNode = 0;

	if(__canTraverse(origin, action))
	{
		{ SYNC(_lock)

			if(origin == _fromNode)
			{
				retNode = _toNode;
			}
			else
			{
				retNode = _fromNode;
			}
		}

		// Don't call functions inside sync blocks.
		if(!retNode -> incrRef()) retNode = 0;
	}

	// Do energy accounting. Remember this is a cyclic graph and the energy accounting system helps mitigate infinite cycles.
	if(retNode) action -> __consumeEnergy(_energyCost);

	return retNode;
}