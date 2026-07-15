#include "GraphNode.hpp"

#include <iostream>

#include "GraphAction.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "GraphHandle.hpp"
#include "GraphHive.hpp"
#include "GraphNodeScheduledActionThreadPoolWorkUnit.hpp"

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

unsigned GraphNode::getId()
{
	return _id;
}

GraphHandle<GraphHive> GraphNode::getHive()
{
	{ SYNC(_lock)

		return GraphHandle<GraphHive>(_hive);
	}
}

bool GraphNode::setHive(GraphHandle<GraphHive> hive)
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

int GraphNode::createEdge(GraphHandle<GraphNode>& connectTo)
{
	// Default value indicates edge wasn't created.
	int retHandle = -1;

	if(connectTo.isValid())
	{
		{ SYNC(_lock)

			if(_decoupled || !(_edgeCount < EDGE_ARRAY_SIZE)) return -1;

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

GraphHandle<GraphEdge> GraphNode::traverse(GraphAction& action)
{
	// Using a handle guarantees that the edge will be availble.
	GraphHandle<GraphEdge> edgeToTraverse(0);

	std::vector<GraphHandle<GraphEdge>> edgesToCheck;
	unsigned numEdgesToCheck = 0;

	{ SYNC(_lock)

		if(!_decoupled)
		{
			for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
			{
				if(_edges[index] != 0)
				{
					GraphHandle<GraphEdge> edgeHandle(_edges[index]);

					edgesToCheck.push_back(edgeHandle);
					numEdgesToCheck++;
				}
			}
		}
	}

	for(unsigned index = 0; index < numEdgesToCheck; index++)
	{
		GraphHandle<GraphEdge> edgeHandleToCheck = edgesToCheck[index];

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
	if(_pokeEnabled) std::cout << "Node with id:" << getId() << " was poked." << std::endl;

	bool pokeEnabled;

	{ SYNC(_lock)

		pokeEnabled = _pokeEnabled;
	}

	// _poked() runs a node's poke script, which must not happen while holding _lock (see the note above
	// __executeScheduledActionWorkUnit() for the same kind of re-entrancy hazard).
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

bool GraphNode::scheduleAction(GraphHandle<GraphAction> action)
{
	if(!action.isValid()) return false;

	bool submitWorkUnit = false;

	{ SYNC(_lock)

		// Once decoupled, a node can no longer have actions applied to it.
		if(_decoupled) return false;

		_scheduledActions.push(action);

		// Only the transition from an idle queue to a non-idle queue needs to kick off a work unit. Any
		// other push will be picked up by the work unit currently draining the queue.
		if(!_scheduledActionProcessing)
		{
			_scheduledActionProcessing = true;
			submitWorkUnit = true;
		}
	}

	if(submitWorkUnit && !__executeScheduledActionWorkUnit())
	{
		{ SYNC(_lock)

			_scheduledActionProcessing = false;
		}
	}

	return true;
}

void GraphNode::processScheduledAction(bool abort)
{
	GraphHandle<GraphAction> action(0);
	bool moreWork = false;

	if(!abort)
	{
		{ SYNC(_lock)

			if(!_scheduledActions.empty())
			{
				action = _scheduledActions.front();
				_scheduledActions.pop();
			}
		}

		// Applied outside of the lock as this could re-enter this node, eg via _emitAction. _scheduledActionProcessing
		// is deliberately left true across this call so that any action concurrently pushed by scheduleAction is left
		// queued rather than being dispatched to a new work unit, which would let it jump ahead of this one.
		if(action.isValid()) action.getInstance() -> applyScheduled(GraphHandle<GraphNode>(this));

		{ SYNC(_lock)

			moreWork = !_scheduledActions.empty();

			if(!moreWork) _scheduledActionProcessing = false;
		}
	}
	else
	{
		// The work unit was never given a thread. Leave the queue untouched and stop processing here; the
		// next call to scheduleAction will resume draining the queue from where it was left off.
		{ SYNC(_lock)

			_scheduledActionProcessing = false;
		}
	}

	if(moreWork && !__executeScheduledActionWorkUnit())
	{
		{ SYNC(_lock)

			_scheduledActionProcessing = false;
		}
	}
}

bool GraphNode::__executeScheduledActionWorkUnit()
{
	// Note: This must not be called while holding _lock, as executeWorkUnit may invoke abort() synchronously,
	//       which in turn calls back into processScheduledAction and re-acquires _lock.

	bool submitted = false;

	GraphHandle<GraphHive> hive = getHive();

	if(hive.isValid())
	{
		try
		{
			submitted = hive.getInstance() -> executeWorkUnit(new GraphNodeScheduledActionThreadPoolWorkUnit(this));
		}
		catch(std::bad_alloc& ex)
		{
			submitted = false;
		}
	}

	return submitted;
}

