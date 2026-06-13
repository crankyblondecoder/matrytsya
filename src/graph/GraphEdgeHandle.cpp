#include "GraphEdgeHandle.hpp"

#include "GraphEdge.hpp"

GraphEdgeHandle::GraphEdgeHandle(GraphEdge* edge)
{
	if(edge && edge -> incrRef())
	{
		_referencedEdge = edge;
	}
	else
	{
		_referencedEdge = 0;
	}
}

GraphEdgeHandle::GraphEdgeHandle(const GraphEdgeHandle& copyFrom)
{
	if(copyFrom._referencedEdge && copyFrom._referencedEdge -> incrRef())
	{
		_referencedEdge = copyFrom._referencedEdge;
	}
	else
	{
		_referencedEdge = 0;
	}
}

GraphEdgeHandle& GraphEdgeHandle::operator= (const GraphEdgeHandle& copyFrom)
{
	if(this == &copyFrom) return *this;

	if(_referencedEdge) _referencedEdge -> decrRef();

	if(copyFrom._referencedEdge && copyFrom._referencedEdge -> incrRef())
	{
		_referencedEdge = copyFrom._referencedEdge;
	}
	else
	{
		_referencedEdge = 0;
	}

	return *this;
}

GraphEdgeHandle::~GraphEdgeHandle()
{
	if(_referencedEdge) _referencedEdge -> decrRef();

	_referencedEdge = 0;
}

GraphEdge* GraphEdgeHandle::getEdge()
{
	return _referencedEdge;
}

bool GraphEdgeHandle::isValid()
{
	return _referencedEdge != 0;
}
