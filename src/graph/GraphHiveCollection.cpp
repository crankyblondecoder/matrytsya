#include "GraphException.hpp"
#include "GraphHive.hpp"
#include "GraphHiveCollection.hpp"
#include "GraphHiveHandle.hpp"

void GraphHiveCollection::addHive(GraphHiveHandle hiveHandle)
{
	if(hiveHandle.isValid())
	{
		const std::string hiveName = hiveHandle.getHive() -> getName();

		if(_hives.contains(hiveName))
		{
			throw GraphException(GraphException::DUPLICATE_HIVE);
		}
		else
		{
			_hives[hiveName] = hiveHandle;
		}
	}
	else
	{
		throw GraphException(GraphException::INVALID_HIVE_HANDLE);
	}
}

void GraphHiveCollection::shutdown()
{
	for(auto& [hiveName, hiveHandle] : _hives)
	{
		if(hiveHandle.isValid()) hiveHandle.getHive() -> shutdown();
	}
}

