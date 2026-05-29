#include "../thread/thread.hpp"

#include "GraphHive.hpp"
#include "GraphNode.hpp"

GraphHive::~GraphHive()
{
	for(GraphNode* node : _nodes)
	{
		node -> decouple();
		node -> decrRef();
	}
}

GraphHive::GraphHive()
{
}

unsigned GraphHive::addNode(GraphNode* node)
{
	{ SYNC(_lock)

		for(unsigned index = 0; index < _nodes.size(); )
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

		if(nodeIndex < _nodes.size() && _nodes[nodeIndex])
		{
			_nodes[nodeIndex] -> decouple();
			_nodes[nodeIndex] -> decrRef();
			_nodes[nodeIndex] = 0;
		}
	}
}

