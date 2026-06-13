#include "GraphNode.hpp"

#include <new>

#include "GraphAction.hpp"
#include "GraphEdge.hpp"
#include "GraphEdgeHandle.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveHandle.hpp"
#include "GraphNodeHandle.hpp"

GraphNode::~GraphNode()
{
	// Remove all edges.

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
	{
		if(_edges[index] != 0)
		{
			_edges[index] -> decrRef();
		}
	}
}

GraphNode::GraphNode(GraphHiveHandle& hive) : _hive(hive)
{
	_decoupled = false;
	_edgeCount = 0;
	_actionEnergyCost = 1;
	_initialised = false;

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
	{
		_edges[index] = 0;
		_edgeAlloc[index] = false;
	}

	if(_hive.isValid())	_hive.getHive() -> addNode(this);
}

GraphHiveHandle GraphNode::getHive()
{
	return GraphHiveHandle(_hive);
}

void GraphNode::decouple()
{
	GraphEdge* edgesToDelete[EDGE_ARRAY_SIZE];

	{ SYNC(_lock)

		// This must happen first to stop edges being added and remove edge not complaining about bad handles.
		_decoupled = true;

		for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
		{
			if(_edges[index])
			{
				edgesToDelete[index] = __removeEdge(index);
			}
			else
			{
				edgesToDelete[index] = 0;
			}
		}
	}

	// The edges must be deleted outside the lock so that potential re-entry from the edge removing a ref count
	// doesn't occur.
	for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
	{
		if(edgesToDelete[index]) edgesToDelete[index] -> decrRef();
	}
}

unsigned GraphNode::getEnergyCost()
{
	{ SYNC(_lock)

		return _actionEnergyCost;
	}
}

void GraphNode::_setEnergyCost(unsigned cost)
{
	{ SYNC(_lock)

		_actionEnergyCost = cost;
	}
}

int GraphNode::createEdge(GraphNodeHandle& connectTo)
{
	// Default value indicates edge wasn't created.
	int retHandle = -1;

	{ SYNC(_lock)

		if(_decoupled || !(_edgeCount < EDGE_ARRAY_SIZE)) return -1;
	}

	if(connectTo.isValid())
	{
		{ SYNC(_lock)

			if(_decoupled) return -1;

			// Find first available edge slot.
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(!_edgeAlloc[index])
				{
					retHandle = index;
					_edgeAlloc[index] = true;
					_edgeCount++;
					break;
				}
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
				{ SYNC(_lock)

					_edgeAlloc[retHandle] = false;
					_edgeCount--;
				}

				throw GraphException(GraphException::EDGE_BAD_ALLOC);
			}
			catch(...)
			{
				{ SYNC(_lock)

					_edgeAlloc[retHandle] = false;
					_edgeCount--;
				}

				throw;
			}

			bool deleteEdge = false;

			if(edge -> isComplete())
			{
				{ SYNC(_lock)

					if(_decoupled)
					{
						_edgeAlloc[retHandle] = false;
						_edgeCount--;
						retHandle = -1;
						deleteEdge = true;
					}
					else
					{
						_edges[retHandle] = edge;
					}
				}
			}
			else
			{
				{ SYNC(_lock)

					_edgeAlloc[retHandle] = false;
					_edgeCount--;
				}

				retHandle = -1;
				deleteEdge = true;
			}

			// Make sure this is done outside of a sync lock because it could potentially trigger a node delete.
			if(deleteEdge) edge -> decrRef();
		}
	}

	return retHandle;
}

void GraphNode::removeEdge(int edgeHandle)
{
	GraphEdge* edgeToDelete = 0;

    { SYNC(_lock)

		edgeToDelete = __removeEdge(edgeHandle);
	}

	if(edgeToDelete) edgeToDelete -> decrRef();
}

GraphEdge* GraphNode::__removeEdge(int edgeHandle)
{
	// Note: This function needs to be externally synchronised.

	GraphEdge* edge = 0;

	// The test for a null pointer in the edges array is to detect if an edge is in the process of being allocated.

	if(edgeHandle < EDGE_ARRAY_SIZE && edgeHandle >= 0 && _edgeAlloc[edgeHandle] && _edges[edgeHandle])
	{
		edge = _edges[edgeHandle];
		_edges[edgeHandle] = 0;
		_edgeAlloc[edgeHandle] = false;
		_edgeCount--;
	}
	else
	{
		// Decoupling can be triggered when an edge has been added incompletely. This should not trigger this exception.
		if(!_decoupled) throw GraphException(GraphException::INVALID_EDGE_HANDLE);
	}

	return edge;
}

void GraphNode::referredTo(GraphEdge* edge)
{
	bool runInit = false;

	{ SYNC(_lock)

		if(!_initialised)
		{
			runInit = true;
			_initialised = true;
		}
	}

	if(runInit) _init();
}

GraphNodeHandle GraphNode::traverse()
{
	// Using a handle guarantees that the edges refcount will be decr.
	GraphEdgeHandle edgeToTraverse(0);

	{ SYNC(_lock)

		if(!_decoupled)
		{
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(_edges[index] != 0)
				{
					edgeToTraverse = _edges[index];
					break;
				}
			}
		}
	}

	if(edgeToTraverse.isValid())
	{
		return (edgeToTraverse.getEdge()) -> traverse();
	}

	// If gets to here then a non-valid handle must be returned.
	return GraphNodeHandle(0);
}

void GraphNode::_emitAction(GraphAction* action)
{
	action -> start();
}

