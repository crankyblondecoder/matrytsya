#include <stdexcept>

#include "../thread/ThreadException.hpp"
#include "../thread/ThreadPool.hpp"
#include "GraphHive.hpp"
#include "GraphNode.hpp"

GraphHive::~GraphHive()
{
}

GraphHive::GraphHive(unsigned numThreads)
{
	_active = false;
	_threadPool = 0;

	try
	{
		_threadPool = new ThreadPool(numThreads);
		bool isActive = _threadPool -> waitOnBecomingActive();

		if(!isActive)
		{
			_threadPool -> shutdown();
			_threadPool = 0;

			std::string msg = "Critical error: thread pool did not become active.";
			throw std::runtime_error(msg);
		}
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: thread pool creation failed -> " +
			ex.getSubsystemErrorString();

		throw std::runtime_error(msg);
	}

	_active = true;
}

bool GraphHive::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	{ SYNC(_lock)

		if(!_active) return false;
	}

	return _threadPool -> executeWorkUnit(workUnit);
}

void GraphHive::shutdown()
{
	{ SYNC(_lock)

		_active = false;
	}

	for(GraphNode* node : _nodes)
	{
		node -> decouple();
		node -> decrRef();
	}

	if(_threadPool)
	{
		_threadPool -> shutdown();
		delete _threadPool;
	}

	// Allow this to be deleted.
	decrRef();
}

unsigned GraphHive::addNode(GraphNode* node)
{
	{ SYNC(_lock)

		if(!_active) return -1;

		for(unsigned index = 0; index < _nodes.size(); index++)
		{
			if(!_nodes[index])
			{
				_nodes[index] = node;
				return index;
			}
		}

		// No spare slot available.

		_nodes.push_back(node);
		return _nodes.size() - 1;
	}
}

void GraphHive::removeNode(unsigned nodeIndex)
{
	{ SYNC(_lock)

		if(!_active) return;

		if(nodeIndex < _nodes.size() && _nodes[nodeIndex])
		{
			_nodes[nodeIndex] -> decouple();
			_nodes[nodeIndex] -> decrRef();
			_nodes[nodeIndex] = 0;
		}
	}
}

void GraphHive::enumerateThreadPool(unsigned numTabs)
{
	{ SYNC(_lock)

		if(_threadPool) _threadPool -> enumerateState(numTabs);
	}
}
