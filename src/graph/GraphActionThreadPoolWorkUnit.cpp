#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"

GraphActionThreadPoolWorkUnit::~GraphActionThreadPoolWorkUnit()
{
	if(_graphAction)
	{
		_graphAction -> __abortWork();
		_graphAction -> decrRef();
	}
}

GraphActionThreadPoolWorkUnit::GraphActionThreadPoolWorkUnit(GraphAction* graphAction)
{
	if(graphAction -> incrRef())
	{
		_graphAction = graphAction;
	}
	else
	{
		_graphAction = 0;
		graphAction -> __abortWork();
	}
}

void GraphActionThreadPoolWorkUnit::work()
{
	if(_graphAction)
	{
		_graphAction -> __work();

		_graphAction -> decrRef();
		_graphAction = 0;
	}
}

void GraphActionThreadPoolWorkUnit::abort()
{
	if(_graphAction)
	{
		_graphAction -> __abortWork();

		_graphAction -> decrRef();
		_graphAction = 0;
	}
}