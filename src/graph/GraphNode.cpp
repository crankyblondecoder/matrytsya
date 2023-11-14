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

	_graphHandle = graph -> __addNode(this);
}

int GraphNode::getMaxNumAttachedEdges()
{
	return EDGE_ARRAY_SIZE;
}

bool GraphNode::formEdgeTo(GraphNodeHandle& handle, unsigned long traversalFlags)
{
	bool edgeFormed = false;

	if(handle.isValid())
	{
		GraphNode* linkTo = handle.getNode();

		if(linkTo)
		{
			edgeFormed = __formEdge(this, linkTo, traversalFlags);
		}
	}

	return edgeFormed;
}

bool GraphNode::__formEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags)
{
	bool decoupling;

	{ SYNC(_lock)

		decoupling = _decoupling;
	}

	bool success = false;

	if(!decoupling)
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

			// Release automatic construction reference. Nodes will ref the edge again if successfully attached.
			// If edge was incomplete and subsequently detached, it should be automatically deleted at this point.
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

	if(_edgeCount == 0) _detached();
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

	_graph -> __removeNode(_graphHandle);

	decrRef();
}

GraphNode* GraphNode::__traverse(GraphAction* action)
{
	GraphNode* retNode = 0;

	bool decoupling;

	{ SYNC(_lock)

		decoupling = _decoupling;
	}

	if(!decoupling)
	{
		GraphEdge* foundEdge = __findEdgeToTraverse(action);

		if(foundEdge)
		{
			// Get a ref counted node from the edge.
			retNode = foundEdge -> __traverse(this, action);

			// The edge will have had its ref count increased when passed from find to traverse.
			foundEdge -> decrRef();
		}
	}

	// Assume this node was referenced. This assumption can only be made because this function is private and should only be
	// called by GraphAction. Also this is called at the end because it may very well instigate the deletion of this node in
	// the circumstance where this node has no edges.
	decrRef();

	return retNode;
}

void GraphNode::__wontTraverse()
{
	// Assume this was referenced. This assumption can only be made because this function is private and should only be
	// called by GraphAction.
	decrRef();
}

void GraphNode::_emitAction(GraphAction* action)
{
	bool decoupling;

	{ SYNC(_lock)

		decoupling = _decoupling;
	}

	// There is no point in going any further if this is in the process of decoupling.

	if(!decoupling)
	{
		if(incrRef())
		{
			action -> start(this);
		}
	}
}
