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

	struct TypeName
	{
		/// Name this type is exposed under.
		const char* name;

		/// Type that name stands for.
		GraphNodeType type;
	};

	// Every GraphNodeType, mapped to the name it's exposed under. A type absent from here cannot be
	// round-tripped through GraphNode::typeName()/typeFromName(), so this must be extended whenever
	// GraphNodeType is.
	const TypeName TYPE_NAMES[] =
	{
		{"GRAPH_NODE", GraphNodeType::GRAPH_NODE},
		{"PING_NODE", GraphNodeType::PING_NODE},
		{"SCENE_GEOMETRY_NODE", GraphNodeType::SCENE_GEOMETRY_NODE},
		{"SCENE_TRANSFORM_NODE", GraphNodeType::SCENE_TRANSFORM_NODE},
		{"SCRIPT_NODE", GraphNodeType::SCRIPT_NODE},
		{"SCENE_GEOMETRY_SCRIPT_NODE", GraphNodeType::SCENE_GEOMETRY_SCRIPT_NODE},
		{"SCENE_TRANSFORM_SCRIPT_NODE", GraphNodeType::SCENE_TRANSFORM_SCRIPT_NODE},
		{"SCENE_ROOT_NODE", GraphNodeType::SCENE_ROOT_NODE},
		{"TELEPORT_NODE", GraphNodeType::TELEPORT_NODE},
		{"AGENT_NODE", GraphNodeType::AGENT_NODE},
		{"TRIGGER_NODE", GraphNodeType::TRIGGER_NODE}
	};
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

GraphNode::GraphNode()
	: _id { _nextId++ }, _hive(nullptr)
{
}

GraphNodeType GraphNode::getType()
{
	return GraphNodeType::GRAPH_NODE;
}

std::string GraphNode::typeName(GraphNodeType type)
{
	for(const TypeName& entry : TYPE_NAMES)
	{
		if(entry.type == type) return entry.name;
	}

	// Unreachable so long as TYPE_NAMES covers every GraphNodeType member.
	return TYPE_NAMES[0].name;
}

GraphNodeType GraphNode::typeFromName(const std::string& name)
{
	for(const TypeName& entry : TYPE_NAMES)
	{
		if(name == entry.name) return entry.type;
	}

	throw GraphException(GraphException::UNKNOWN_NODE_TYPE_NAME);
}

std::vector<std::string> GraphNode::typeNames()
{
	std::vector<std::string> names;

	for(const TypeName& entry : TYPE_NAMES)
	{
		names.push_back(entry.name);
	}

	return names;
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

		Handle<GraphHive> hive = getHive();

		if(hive.isValid()) hive.getInstance() -> nodeEdgesChanged();
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

	if(edgeToDelete)
	{
		edgeToDelete -> decrRef();

		Handle<GraphHive> hive = getHive();

		if(hive.isValid()) hive.getInstance() -> nodeEdgesChanged();
	}
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

	// First traversable edge found that carries no action flags. Only used if no flagged edge can be traversed.
	Handle<GraphEdge> wildcardEdgeToTraverse(0);

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
			// An edge that names the actions it accepts is a deliberate route for this action, so it is taken
			// ahead of a wildcard edge that merely fails to exclude it. The wildcard is held as a fallback
			// rather than taken now, in case a flagged edge is found further along the array.
			if(edgeHandleToCheck.getInstance() -> hasActionFlags())
			{
				edgeToTraverse = edgeHandleToCheck;
				break;
			}

			if(!wildcardEdgeToTraverse.isValid()) wildcardEdgeToTraverse = edgeHandleToCheck;
		}
	}

	if(!edgeToTraverse.isValid()) edgeToTraverse = wildcardEdgeToTraverse;

	return edgeToTraverse;
}

std::vector<Handle<GraphEdge>> GraphNode::getEdges()
{
	std::vector<Handle<GraphEdge>> edges;

	{ SYNC(_lock)

		if(!_decoupled)
		{
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(_edges[index] != 0) edges.push_back(Handle<GraphEdge>(_edges[index]));
			}
		}
	}

	return edges;
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

void GraphNode::_emitAction(GraphAction* action)
{
	action -> start();
}

