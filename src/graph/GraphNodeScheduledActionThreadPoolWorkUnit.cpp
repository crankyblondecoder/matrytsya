#include "GraphNodeScheduledActionThreadPoolWorkUnit.hpp"

#include "GraphNode.hpp"

GraphNodeScheduledActionThreadPoolWorkUnit::~GraphNodeScheduledActionThreadPoolWorkUnit()
{
	if(_node)
	{
		_node -> processScheduledAction(true);
		_node -> decrRef();
	}
}

GraphNodeScheduledActionThreadPoolWorkUnit::GraphNodeScheduledActionThreadPoolWorkUnit(GraphNode* node)
{
	if(node -> incrRef())
	{
		_node = node;
	}
	else
	{
		_node = 0;
		node -> processScheduledAction(true);
	}
}

void GraphNodeScheduledActionThreadPoolWorkUnit::work()
{
	if(_node)
	{
		_node -> processScheduledAction(false);

		_node -> decrRef();
		_node = 0;
	}
}

void GraphNodeScheduledActionThreadPoolWorkUnit::abort()
{
	if(_node)
	{
		_node -> processScheduledAction(true);

		_node -> decrRef();
		_node = 0;
	}
}
