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
	_edgeCount = 0;
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
	// NOTE: This relies on a the ref count to be increased prior to entry to give thread safety. It is _not_ safe
	//       to allow a delete to be triggered here.

	GraphEdge* edge = 0;

    { SYNC(_lock)

		if(edgeHandle < EDGE_ARRAY_SIZE && edgeHandle >= 0 && _edges[edgeHandle])
		{
			edge = _edges[edgeHandle];
			_edges[edgeHandle] = 0;
			_edgeCount--;

			if(_edgeCount == 0 && !_initRefcountRemoved)
			{
				// Last edge removal allows node to be deleted.
				// This covers case of a node only having edges from this to elsewhere but nothing pointing to it.
				decrRef();

				_initRefcountRemoved = true;
			}
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
	if(!_initRefcountRemoved)
	{
		decrRef();
		_initRefcountRemoved = true;
	}
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

