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

		if(_threadPool)
		{
			_threadPool -> shutdown();
			_threadPool = 0;
		}

		throw std::runtime_error(msg);
	}

	_active = true;
}

bool GraphHive::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	{ SYNC(_lock)

		if(!_active)
		{
			delete workUnit;
			return false;
		}

		return _threadPool -> executeWorkUnit(workUnit);
	}
}

void GraphHive::shutdown()
{
	{ SYNC(_lock)

		// After construction, assume that the active flag can only be set to false by shutdown.
		if(!_active) return;

		_active = false;
	}

	for(GraphNode* node : _nodes)
	{
		if(node)
		{
			node -> decouple();
			node -> decrRef();
		}
	}

	if(_threadPool)
	{
		_threadPool -> shutdown();
		delete _threadPool;
		_threadPool = 0;
	}

	// Allow this to be deleted.
	decrRef();
}

int GraphHive::addNode(GraphNode* node)
{
	int retIndex = -1;

	{ SYNC(_lock)

		if(_active)
		{
			for(unsigned index = 0; index < _nodes.size(); index++)
			{
				if(!_nodes[index])
				{
					_nodes[index] = node;
					retIndex = index;
					break;
				}
			}

			if(retIndex == -1)
			{
				// No spare slot available.
				_nodes.push_back(node);

				retIndex = _nodes.size() - 1;
			}
		}
	}

	if(retIndex != -1) node -> setHive(GraphHiveHandle(this));

	return retIndex;
}

void GraphHive::removeNode(unsigned nodeIndex)
{
	GraphNode* nodeToDecouple = 0;

	{ SYNC(_lock)

		if(!_active) return;

		if(nodeIndex < _nodes.size() && _nodes[nodeIndex])
		{
			nodeToDecouple = _nodes[nodeIndex];
			_nodes[nodeIndex] = 0;
		}
	}

	if(nodeToDecouple)
	{
		nodeToDecouple -> decouple();
		nodeToDecouple -> decrRef();
	}
}

void GraphHive::enumerateThreadPool(unsigned numTabs)
{
	{ SYNC(_lock)

		if(_threadPool) _threadPool -> enumerateState(numTabs);
	}
}
