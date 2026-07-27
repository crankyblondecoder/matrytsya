#include "GraphNode.hpp"

#include <iostream>

#include "GraphAction.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "../util/Handle.hpp"
#include "GraphHive.hpp"

namespace
{
	const char* pokeTypeName(GraphPoke::PokeType type)
	{
		switch(type)
		{
			case GraphPoke::PokeType::HIT:          return "HIT";
			case GraphPoke::PokeType::GRAB:         return "GRAB";
			case GraphPoke::PokeType::DRAG:         return "DRAG";
			case GraphPoke::PokeType::HOVER_ENTER:  return "HOVER_ENTER";
			case GraphPoke::PokeType::HOVER_LEAVE:  return "HOVER_LEAVE";
		}

		return "HIT";
	}
}

std::atomic<unsigned> GraphNode::_nextId{0};

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

GraphNode::GraphNode() : _id { _nextId++ }, _hive(nullptr)
{
}

GraphNode::Type GraphNode::getType()
{
	return Type::GRAPH_NODE;
}

unsigned GraphNode::getId()
{
	return _id;
}

Handle<GraphHive> GraphNode::getHive()
{
	{ SYNC(_lock)

		return Handle<GraphHive>(_hive);
	}
}

bool GraphNode::setHive(Handle<GraphHive> hive)
{
	if(!hive.isValid()) return false;

	{ SYNC(_lock)

		// Once a node is decoupled, it can never be part of another hive.
		if(_decoupled) return false;

		_hive = hive;
	}

	return true;
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

Handle<GraphEdge> GraphNode::createEdge(Handle<GraphNode>& connectTo, std::vector<unsigned long> actionFlags)
{
	int retIndex = -1;
	Handle<GraphEdge> retHandle(0);

	if(connectTo.isValid())
	{
		{ SYNC(_lock)

			if(_decoupled || !(_edgeCount < EDGE_ARRAY_SIZE)) return retHandle;

			// Find first available edge slot.
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(!_edgeAlloc[index])
				{
					retIndex = index;
					_edgeAlloc[index] = true;
					_edgeCount++;
					break;
				}
			}
		}

		if(retIndex > -1)
		{
			GraphEdge* edge = 0;

			try
			{
				// Edges are immutable and should not exist if not fully connected.
				edge = new GraphEdge(connectTo, actionFlags);
			}
			catch(std::bad_alloc& ex)
			{
				{ SYNC(_lock)

					_edgeAlloc[retIndex] = false;
					_edgeCount--;
				}

				throw GraphException(GraphException::EDGE_BAD_ALLOC);
			}
			catch(...)
			{
				{ SYNC(_lock)

					_edgeAlloc[retIndex] = false;
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
						_edgeAlloc[retIndex] = false;
						_edgeCount--;
						retIndex = -1;
						deleteEdge = true;
					}
					else
					{
						_edges[retIndex] = edge;
						edge -> incrRef();
					}
				}

				if(!deleteEdge)
				{
					edge -> decrRef();
				}
 			}
			else
			{
				{ SYNC(_lock)

					_edgeAlloc[retIndex] = false;
					_edgeCount--;
				}

				retIndex = -1;
				deleteEdge = true;
			}

			// Make sure this is done outside of a sync lock because it could potentially trigger a node delete.
			if(deleteEdge) edge -> decrRef();
		}
	}

	if(retIndex != -1)
	{
		{ SYNC(_lock)

			retHandle = _edges[retIndex];
		}
	}

	return retHandle;
}

void GraphNode::removeEdge(Handle<GraphEdge> edgeHandle)
{
	GraphEdge* edgeToDelete = 0;

    { SYNC(_lock)

		int foundIndex = -1;

		for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
		{
			if(_edges[index] && edgeHandle == _edges[index])
			{
				foundIndex = index;
				break;
			}
		}

		if(foundIndex >= 0) edgeToDelete = __removeEdge(foundIndex);
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

Handle<GraphEdge> GraphNode::traverse(GraphAction& action)
{
	// Using a handle guarantees that the edge will be availble.
	Handle<GraphEdge> edgeToTraverse(0);

	std::vector<Handle<GraphEdge>> edgesToCheck;
	unsigned numEdgesToCheck = 0;

	{ SYNC(_lock)

		if(!_decoupled)
		{
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(_edges[index] != 0)
				{
					Handle<GraphEdge> edgeHandle(_edges[index]);

					edgesToCheck.push_back(edgeHandle);
					numEdgesToCheck++;
				}
			}
		}
	}

	for(unsigned index = 0; index < numEdgesToCheck; index++)
	{
		Handle<GraphEdge> edgeHandleToCheck = edgesToCheck[index];

		// Both the edge and the action must allow traversal.
		if(edgeHandleToCheck.getInstance() -> canTraverse(action.getEdgeTraversalFlags()) &&
				action.canTraverseEdge(edgeHandleToCheck))
		{
			edgeToTraverse = edgeHandleToCheck;
			break;
		}
	}

	return edgeToTraverse;
}

void GraphNode::_emitAction(GraphAction* action)
{
	action -> start();
}

void GraphNode::poke(GraphPoke pokeToProcess)
{
	if(_pokeEnabled) std::cout << "Node with id:" << getId() << " was poked with a " <<
		pokeTypeName(pokeToProcess.getType()) << " poke." << std::endl;

	bool pokeEnabled;

	{ SYNC(_lock)

		pokeEnabled = _pokeEnabled;
	}

	// Silently discard the poke if poking isn't enabled.
	if(pokeEnabled) _poked(pokeToProcess);
}

bool GraphNode::getPokeEnabled()
{
	return _pokeEnabled;
}

void GraphNode::setPokeEnabled(bool enable)
{
	{ SYNC(_lock)

		_pokeEnabled = enable;
	}
}

bool GraphNode::getActionable()
{
	// Once decoupled, a node can no longer have actions applied to it.
	return !_decoupled;
}

