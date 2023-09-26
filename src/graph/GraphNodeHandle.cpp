#include "GraphNodeHandle.hpp"

GraphNodeHandle::GraphNodeHandle(GraphNode* node)
{
	if(node -> incrRef()) {

		_referencedNode = node;

	} else {

		_referencedNode = 0;
	}
}

GraphNodeHandle::~GraphNodeHandle()
{

	if(_referencedNode) _referencedNode -> decrRef();
}

GraphNode* GraphNodeHandle::getNode()
{
	return _referencedNode;
}

bool GraphNodeHandle::isValid()
{
	return _referencedNode != 0;
}
