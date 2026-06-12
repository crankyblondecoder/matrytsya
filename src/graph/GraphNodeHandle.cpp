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
	if(copyFrom._referencedNode -> incrRef())
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
	if(_referencedNode) _referencedNode -> decrRef();

	if(copyFrom._referencedNode -> incrRef())
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
	if(_referencedNode) _referencedNode -> decrRef();

	_referencedNode = 0;
}

GraphNode* GraphNodeHandle::getNode()
{
	if(_referencedNode -> incrRef())
	{
		return _referencedNode;
	}

	return 0;
}

bool GraphNodeHandle::isValid()
{
	return _referencedNode != 0;
}
