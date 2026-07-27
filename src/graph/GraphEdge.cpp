#include "GraphEdge.hpp"
#include "../util/Handle.hpp"
#include "GraphNode.hpp"

std::atomic<unsigned> GraphEdge::_nextId{0};

GraphEdge::~GraphEdge()
{
	if(_toNode) delete _toNode;
}

GraphEdge::GraphEdge(Handle<GraphNode>& fromNode, std::vector<unsigned long> actionFlags) : _id { _nextId++ }
{
	_toNode = 0;

	// Try and make a copy of the handle.
	Handle<GraphNode>* newHandle = new Handle<GraphNode>(fromNode);

	if(newHandle -> isValid())
	{
		_toNode = newHandle;
	}
	else
	{
		delete newHandle;
	}

	for(unsigned long flag : actionFlags)
	{
		_actionFlags |= flag;
	}
}

unsigned GraphEdge::getId()
{
	return _id;
}

bool GraphEdge::isComplete()
{
	return _toNode != 0;
}

Handle<GraphNode> GraphEdge::traverse()
{
	if(_toNode)
	{
		Handle<GraphNode> retHandle(*_toNode);
		return retHandle;
	}

	return 0;
}

void GraphEdge::addActionFlag(unsigned long actionFlag)
{
	_actionFlags |= actionFlag;
}

bool GraphEdge::canTraverse(unsigned long actionFlags)
{
	// No action flags set defaults to anything can traverse.
	if(!_actionFlags) return true;

	// This now checks for specific flags being present.
	return actionFlags & _actionFlags;
}

std::string GraphEdge::getDescription()
{
	return _description;
}

void GraphEdge::setDescription(std::string description)
{
	_description = description;
}

