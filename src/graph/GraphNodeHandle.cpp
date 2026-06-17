#include "GraphNodeHandle.hpp"

#include "GraphNode.hpp"

GraphNodeHandle::GraphNodeHandle(GraphNode* node)
{
	if(node && node -> incrRef())
	{
		_referencedNode = node;
	}
	else
	{
		_referencedNode = 0;
	}
}

GraphNodeHandle::GraphNodeHandle(const GraphNodeHandle& copyFrom)
{
	if(copyFrom._referencedNode && copyFrom._referencedNode -> incrRef())
	{
		_referencedNode = copyFrom._referencedNode;
	}
	else
	{
		_referencedNode = 0;
	}
}

GraphNodeHandle& GraphNodeHandle::operator= (const GraphNodeHandle& copyFrom)
{
	if(this == &copyFrom) return *this;

	if(_referencedNode) _referencedNode -> decrRef();

	if(copyFrom._referencedNode && copyFrom._referencedNode -> incrRef())
	{
		_referencedNode = copyFrom._referencedNode;
	}
	else
	{
		_referencedNode = 0;
	}

	return *this;
}

GraphNodeHandle::~GraphNodeHandle()
{
	clear();
}

GraphNode* GraphNodeHandle::getNode()
{
	return _referencedNode;
}

bool GraphNodeHandle::isValid()
{
	return _referencedNode != 0;
}

void GraphNodeHandle::clear()
{
	if(_referencedNode) _referencedNode -> decrRef();

	_referencedNode = 0;
}

