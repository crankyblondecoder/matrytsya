#include "GraphNode.hpp"

#include <new>

#include "GraphAction.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "GraphNodeHandle.hpp"

GraphNode::~GraphNode()
{
	// Remove all edges.

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
	{
		if(_edges[index] != 0)
		{
			delete _edges[index];
		}
	}
}

GraphNode::GraphNode()
{
	_initRefcountRemoved = false;
	_actionEnergyCost = 1;

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++) _edges[index] = 0;
}

unsigned GraphNode::getEnergyCost()
{
	return _actionEnergyCost;
}

void GraphNode::_setEnergyCost(unsigned cost)
{
	_actionEnergyCost = cost;
}

int GraphNode::createEdge(GraphNodeHandle& connectTo)
{
	int retHandle = -1;

	if(connectTo.isValid())
	{
		{ SYNC(_lock)

			// Make sure there is a spare edge slot available.
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(_edges[index] == 0)
				{
					retHandle = index;
					break;
				}
			}

			if(retHandle > -1)
			{
				GraphEdge* edge = 0;

				try
				{
					// Edges are immutable and should not exist if not fully connected.
					edge = new GraphEdge(connectTo);
				}
				catch(std::bad_alloc& ex)
				{
					throw GraphException(GraphException::EDGE_BAD_ALLOC);
				}

				if(edge && edge -> isComplete())
				{
					_edges[retHandle] = edge;
				}
			}
		}
	}

	return retHandle;
}

void GraphNode::removeEdge(int edgeHandle)
{
	GraphEdge* edge = 0;

    { SYNC(_lock)

		// TODO Handle case of all edges being removed and hence initial incrref needs to be removed.

		if(edgeHandle < EDGE_ARRAY_SIZE && edgeHandle >= 0 && _edges[edgeHandle])
		{
			edge = _edges[edgeHandle];
			_edges[edgeHandle] = 0;
		}
		else
		{
			throw GraphException(GraphException::INVALID_EDGE_HANDLE);
		}
    }

	delete edge;
}

GraphNodeHandle GraphNode::traverse()
{
	{ SYNC(_lock)

		for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
		{
			if(_edges[index] != 0)
			{
				return _edges[index] -> traverse();
			}
		}
	}

	// If gets to here then a non-valid handle must be returned.

	return GraphNodeHandle(0);
}

void GraphNode::_emitAction(GraphAction& action)
{
	action.start();
}

