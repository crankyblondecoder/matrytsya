#include "GraphNode.hpp"

#include <new>
#include "GraphEdge.hpp"
#include "GraphException.hpp"

GraphNode::~GraphNode()
{
	// Graph nodes should only be deleted once their reference count goes to zero.
	// Therefore it should _not_ have any edges when deleted because the edges will keep references to the node.
}

GraphNode::GraphNode()
{
	_edgeCount = 0;
	_linearEdgeAllocCount = 0;
	_decoupling = false;

	for(int index = 0; index < EDGE_ARRAY_SIZE; index++) _edges[index] = 0;
}

int GraphNode::getMaxNumAttachedEdges()
{
	return EDGE_ARRAY_SIZE;
}

bool GraphNode::formEdgeTo(GraphNode* toNode, unsigned long traversalFlags)
{
	return formEdge(this, toNode, traversalFlags);
}

bool GraphNode::formEdge(GraphNode* fromNode, GraphNode* toNode, unsigned long traversalFlags)
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

	bool success = false;

	if(edge)
	{
		success = edge -> isComplete();

		// Release automatic construction reference. Nodes will ref the edge again if successfully attached.
		// If edge was incomplete and subsequently detached, it should be automatically deleted at this point.
		edge -> decrRef();
	}

	return success;
}

int GraphNode::attachEdge(GraphEdge* edge)
{
	int retHandle = -1;

	// Before trying to add edge to array of edges. Make sure ref count can be increased to it.
	if(edge -> incrRef()) {

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

void GraphNode::detachEdge(int edgeHandle)
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

void GraphNode::applyAction(GraphAction* action)
{
	// TODO Energy transaction accounting ...

	if(canActionTarget(action))
	{
		action -> apply(this);
	}
}

GraphEdge* GraphNode::getEdgeToTraverse(GraphAction* action)
{
	GraphEdge* curEdge;
	GraphEdge* foundEdge = 0;

	{ SYNC(_lock)

		if(_edgeCount > 0)
		{
			// Don't traverse whole edge array if we really don't need to.
			// _linearEdgeAllocCount never goes above EDGE_ARRAY_SIZE.

			for(unsigned index = 0; index < _linearEdgeAllocCount; index++)
			{
				curEdge = _edges[index];

				if(curEdge && curEdge -> canTraverse(action))
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

	{ SYNC(_lock)

		_decoupling = true;
		maxEdgeCount = _linearEdgeAllocCount;
	}

	// Assume that once the decoupling flag is set to true that the edges array won't gain additional entries but only
	// possibly loose some from other threads.

	for(unsigned index = 0; index < maxEdgeCount; index++)
	{
		// Index is the same as the edge handle.
		// It shouldn't matter if the edge at the index is empty.
		detachEdge(index);
	}
}