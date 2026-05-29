#include "GraphNode.hpp"

#include <new>

#include "GraphAction.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
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

GraphNode::GraphNode(GraphHive& hive) : _hive(hive)
{
	_edgeCount = 0;
	_actionEnergyCost = 1;

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++) _edges[index] = 0;

	hive.addNode(this);
}

void GraphNode::decouple()
{
	// Add this stage simply remove all edges attached to this.

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
	{
		if(_edges[index]) __removeEdge(index);
	}
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

	if(connectTo.isValid() && _edgeCount < EDGE_ARRAY_SIZE)
	{
		{ SYNC(_lock)

			// Find first available edge slot.
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
					_edgeCount++;
				}
			}
		}
	}

	return retHandle;
}

void GraphNode::removeEdge(int edgeHandle)
{
	__removeEdge(edgeHandle);
}

void GraphNode::__removeEdge(int edgeHandle)
{
	// NOTE: This relies on a the ref count to be increased prior to entry to give thread safety. It is _not_ safe
	//       to allow a delete to be triggered here.

	GraphEdge* edge = 0;

    { SYNC(_lock)

		if(edgeHandle < EDGE_ARRAY_SIZE && edgeHandle >= 0 && _edges[edgeHandle])
		{
			edge = _edges[edgeHandle];
			_edges[edgeHandle] = 0;
			_edgeCount--;
		}
		else
		{
			throw GraphException(GraphException::INVALID_EDGE_HANDLE);
		}
    }

	if(edge) delete edge;
}

void GraphNode::referredTo(GraphEdge* edge)
{
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

void GraphNode::_emitAction(GraphAction* action)
{
	action -> start();
}

