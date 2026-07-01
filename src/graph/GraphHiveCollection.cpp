#include "./actions/ActionFactory.hpp"
#include "./actions/SerialisableAction.hpp"
#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveCollection.hpp"
#include "GraphHiveHandle.hpp"
#include "GraphNodeHandle.hpp"

void GraphHiveCollection::addHive(GraphHiveHandle hiveHandle)
{
	if(hiveHandle.isValid())
	{
		std::string hiveName = hiveHandle.getHive() -> getName();

		{ SYNC(_lock)

			for(GraphHiveHandle* handle : _hives)
			{
				if(handle && handle -> isValid() && handle -> getHive() -> getName() == hiveName)
				{
					throw GraphException(GraphException::DUPLICATE_HIVE);
				}
			}

			GraphHiveHandle* newHandle = new GraphHiveHandle(hiveHandle);

			_hives.push_back(newHandle);
		}
	}
	else
	{
		throw GraphException(GraphException::INVALID_HIVE_HANDLE);
	}
}

GraphHiveHandle GraphHiveCollection::getHive(std::string hiveName)
{
	GraphHiveHandle* foundHandle = 0;

	{ SYNC(_lock)

		for(GraphHiveHandle* handle : _hives)
		{
			if(handle && handle -> isValid() && handle -> getHive() -> getName() == hiveName)
			{
				foundHandle = handle;
				break;
			}
		}

		if(foundHandle) return GraphHiveHandle(*foundHandle);
	}

	return GraphHiveHandle(0);
}

void GraphHiveCollection::teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation)
{
	// TODO Teleport to other hosts, i.e. Use the host/port.
	// NOTE: Failure to find a node is _not_ an exception because it is not expected behaviour.

	GraphHiveHandle hiveHandle = getHive(nodeLocation.getHiveName());

	if(!hiveHandle.isValid())
	{
		throw GraphException(GraphException::ACTION_TELEPORT_FAILED);
	}

	GraphNodeHandle nodeHandle = hiveHandle.getHive() -> getNode(nodeLocation.getNodeName());

	if(!nodeHandle.isValid())
	{
		throw GraphException(GraphException::ACTION_TELEPORT_FAILED);
	}

	SerialisableAction* action = ActionFactory::create(nodeHandle, actionPayload);

	action -> start();
}

void GraphHiveCollection::shutdown()
{
	std::vector<GraphHiveHandle*> hivesToShutdown;

	{ SYNC(_lock)

		hivesToShutdown = _hives;

		_hives.clear();
	}

	for(GraphHiveHandle* handle : hivesToShutdown)
	{
		if(handle)
		{
			if(handle -> isValid()) handle -> getHive() -> shutdown();

			delete handle;
		}
	}
}

