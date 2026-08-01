#include "./actions/ActionFactory.hpp"
#include "./actions/SerialisableAction.hpp"
#include "GraphException.hpp"
#include "../util/Handle.hpp"
#include "GraphHive.hpp"
#include "GraphHiveCollection.hpp"
#include "GraphNode.hpp"

void GraphHiveCollection::addHive(Handle<GraphHive> hiveHandle)
{
	if(hiveHandle.isValid())
	{
		std::string hiveName = hiveHandle.getInstance() -> getName();

		{ SYNC(_lock)

			for(Handle<GraphHive>* handle : _hives)
			{
				if(handle && handle -> isValid() && handle -> getInstance() -> getName() == hiveName)
				{
					throw GraphException(GraphException::DUPLICATE_HIVE);
				}
			}

			Handle<GraphHive>* newHandle = new Handle<GraphHive>(hiveHandle);

			_hives.push_back(newHandle);
		}
	}
	else
	{
		throw GraphException(GraphException::INVALID_HIVE_HANDLE);
	}
}

Handle<GraphHive> GraphHiveCollection::getHive(std::string hiveName)
{
	Handle<GraphHive>* foundHandle = 0;

	{ SYNC(_lock)

		for(Handle<GraphHive>* handle : _hives)
		{
			if(handle && handle -> isValid() && handle -> getInstance() -> getName() == hiveName)
			{
				foundHandle = handle;
				break;
			}
		}

		if(foundHandle) return Handle<GraphHive>(*foundHandle);
	}

	return Handle<GraphHive>(0);
}

std::vector<std::string> GraphHiveCollection::getHiveNames()
{
	std::vector<std::string> names;

	{ SYNC(_lock)

		for(Handle<GraphHive>* handle : _hives)
		{
			if(handle && handle -> isValid()) names.push_back(handle -> getInstance() -> getName());
		}
	}

	return names;
}

void GraphHiveCollection::teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation)
{
	// TODO Teleport to other hosts, i.e. Use the host/port.
	// NOTE: Failure to find a node is _not_ an exception because it is not expected behaviour.

	Handle<GraphHive> hiveHandle = getHive(nodeLocation.getHiveName());

	if(!hiveHandle.isValid())
	{
		throw GraphException(GraphException::ACTION_TELEPORT_FAILED);
	}

	Handle<GraphNode> nodeHandle = hiveHandle.getInstance() -> getNode(nodeLocation.getNodeName());

	if(!nodeHandle.isValid())
	{
		throw GraphException(GraphException::ACTION_TELEPORT_FAILED);
	}

	SerialisableAction* action = ActionFactory::create(nodeHandle, actionPayload);

	action -> start();
}

void GraphHiveCollection::shutdown()
{
	std::vector<Handle<GraphHive>*> hivesToShutdown;

	{ SYNC(_lock)

		hivesToShutdown = _hives;

		_hives.clear();
	}

	for(Handle<GraphHive>* handle : hivesToShutdown)
	{
		if(handle)
		{
			if(handle -> isValid()) handle -> getInstance() -> shutdown();

			delete handle;
		}
	}
}

