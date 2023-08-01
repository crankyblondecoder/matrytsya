#include "GraphAction.hpp"
#include "GraphActionThreadPoolWorkUnit.hpp"

GraphActionThreadPoolWorkUnit::~GraphActionThreadPoolWorkUnit()
{
	if(_graphAction) _graphAction -> __abortWork();
}

GraphActionThreadPoolWorkUnit::GraphActionThreadPoolWorkUnit(GraphAction* graphAction)
{
	_graphAction = graphAction;
}

void GraphActionThreadPoolWorkUnit::work()
{
	if(_graphAction) _graphAction -> __work();

	// Action point can NOT be used after this point because the action will have automatically decrRef on it.
	_graphAction = 0;
}

void GraphActionThreadPoolWorkUnit::abort()
{
	if(_graphAction) _graphAction -> __abortWork();

	// Action point can NOT be used after this point because the action will have automatically decrRef on it.
	_graphAction = 0;
}