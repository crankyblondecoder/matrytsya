#include "GraphNode.hpp"

#include <new>
#include "Graph.hpp"
#include "GraphEdge.hpp"
#include "GraphException.hpp"
#include "GraphNodeHandle.hpp"

GraphNode::~GraphNode()
{
	// Graph nodes should only be deleted once their reference count goes to zero.
	// Therefore it should _not_ have any edges when deleted because the edges will keep references to the node.
}

GraphNode::GraphNode(Graph* graph)
{
	_graph = graph;
	_edgeCount = 0;
	_linearEdgeAllocCount = 0;
	_decoupling = false;

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++) _edges[index] = 0;

	// Assume the implicit ref count protects this call because it is within the constructor.
	_graphHandle = graph -> addNode(this);
}

int GraphNode::getMaxNumAttachedEdges()
{
	return EDGE_ARRAY_SIZE;
}

bool GraphNode::formEdgeTo(GraphNodeHandle& handle, unsigned long traversalFlags)
{
	bool edgeFormed = false;

	// Will return ref incr pointer if available.
	GraphNode* linkTo = handle.getNode();

	if(linkTo)
	{
		if(incrRef())
		{
			edgeFormed = __formEdge(this, linkTo, traversalFlags);

			decrRef();
		}

		linkTo -> decrRef();
	}

	return edgeFormed;
}

bool GraphNode::__formEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags)
{
	// Remember that fromNode and toNode are already refIncr by caller.

	bool success = false;

	// Don't bother syncing the test of decoupling at this point because it could change between the sync and the edge
	// generation anyway.

	if(!_decoupling)
	{
		GraphEdge* edge = 0;

		try
		{
			// Edges should be treated as immutable and should not exist if not fully connected.
			edge = new GraphEdge(fromNode, toNode, traversalFlags);
		}
		catch(std::bad_alloc& ex)
		{
			throw GraphException(GraphException::EDGE_BAD_ALLOC);
		}

		if(edge)
		{
			success = edge -> __isComplete();

			// Pointer to edge isn't stored at this time so release automatic construction implicit ref incr. It was
			// constructed here so this is reasonable.
			edge -> decrRef();
		}
	}

	return success;
}

int GraphNode::__addEdge(GraphEdge* edge)
{
	int retHandle = -1;

	// Before trying to add edge to array of edges. Make sure ref count can be increased to it.
	if(edge -> incrRef())
	{
		{ SYNC(_lock)

			if(!_decoupling && _edgeCount < EDGE_ARRAY_SIZE)
			{
				// Short circuit an exhastive search if not needed.
				if(_linearEdgeAllocCount < EDGE_ARRAY_SIZE)
				{
					_edges[_linearEdgeAllocCount] = edge;
					retHandle = _linearEdgeAllocCount++;
				}
				else
				{
					// Should be slot available, look for available edge array index.
					for(int index = 0; index < EDGE_ARRAY_SIZE; index++)
					{
						if(_edges[index] == 0)
						{
							_edges[index] = edge;
							retHandle = index;
							break;
						}
					}
				}

				_edgeCount++;
			}
		}

		// Don't do this inside sync block!!!
		if(retHandle == -1) edge -> decrRef();
	}

    return retHandle;
}

void GraphNode::__removeEdge(int edgeHandle)
{
	GraphEdge* edge = 0;

    { SYNC(_lock)

		if(edgeHandle < EDGE_ARRAY_SIZE && edgeHandle >= 0)
		{
			if(_edges[edgeHandle])
			{
				edge = _edges[edgeHandle];

				// Edge array never shrinks.
				_edges[edgeHandle] = 0;
				_edgeCount--;
			}
		}
		else
		{
			throw GraphException(GraphException::INVALID_EDGE_HANDLE);
		}
    }

	// Don't do this inside sync block!!!
	if(edge) edge -> decrRef();
}

GraphEdge* GraphNode::__findEdgeToTraverse(GraphAction* action)
{
	GraphEdge* curEdge;
	GraphEdge* foundEdge = 0;

	{ SYNC(_lock)

		if(!_decoupling && _edgeCount > 0)
		{
			// Don't traverse whole edge array if we really don't need to.
			// _linearEdgeAllocCount never goes above EDGE_ARRAY_SIZE.

			for(unsigned index = 0; index < _linearEdgeAllocCount; index++)
			{
				curEdge = _edges[index];

				if(curEdge && curEdge -> __canTraverse(this, action))
				{
					if(curEdge -> incrRef())
					{
						foundEdge = curEdge;
						break;
					}
				}
			}
		}
	}

	return foundEdge;
}

void GraphNode::decouple()
{
	unsigned maxEdgeCount;
	bool abort = false;

	{ SYNC(_lock)

		if(_decoupling)
		{
			abort = true;
		}
		else
		{
			_decoupling = true;
			maxEdgeCount = _linearEdgeAllocCount;
		}
	}

	if(abort) return;

	// Assume that once the decoupling flag is set to true that the edges array won't be modified other than by this thread.

	for(unsigned index = 0; index < maxEdgeCount; index++)
	{
		if(_edges[index]) _edges[index] -> __detach();
	}

	_graph -> removeNode(_graphHandle);

	// Must be run before the implicit decr ref (so this can be deleted).
	_decoupled();

	// Delete implicit ref incr so that node can now be deleted.
	decrRef();
}

GraphNode* GraphNode::__traverse(GraphAction* action)
{
	GraphNode* retNode = 0;

	// Get ref incr edge to traverse.
	GraphEdge* foundEdge = __findEdgeToTraverse(action);

	if(foundEdge)
	{
		// Get a ref incr node from the edge.
		retNode = foundEdge -> __traverse(this, action);

		// Finished with edge pointer.
		foundEdge -> decrRef();
	}

	return retNode;
}

void GraphNode::_emitAction(GraphAction* action)
{
	if(!_decoupling)
	{
		if(incrRef())
		{
			action -> __start(this);

			decrRef();
		}
	}
}
