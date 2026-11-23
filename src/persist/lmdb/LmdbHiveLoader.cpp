#include "LmdbHiveLoader.hpp"

#include "LmdbHiveStore.hpp"
#include "lmdbHiveRecords.hpp"
#include "../PersistException.hpp"
#include "../json/JsonHiveLoader.hpp"

#include <map>
#include <set>

namespace
{
	/**
	 * Find a record that the meta record says is there.
	 * @throw PersistException(LMDB_CORRUPT_RECORD) If it is not.
	 */
	const std::string& __requireRecord(const std::map<std::string, std::string>& records, const std::string& key)
	{
		auto it = records.find(key);

		if(it == records.end())
			throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "'" + key + "' is missing");

		return it -> second;
	}

	/**
	 * Reraise a failure to read a record back as the record being corrupt, naming the record and what was wrong
	 * with it, since whoever is told about it is looking at a database rather than a file they wrote.
	 */
	PersistException __corrupt(const std::string& key, PersistException& cause)
	{
		std::string detail = "'" + key + "': " + PersistException::getErrorDescription(cause.getError());

		if(!cause.getDetail().empty()) detail += " (" + cause.getDetail() + ")";

		return PersistException(PersistException::LMDB_CORRUPT_RECORD, detail);
	}
}

LmdbHiveLoader::LmdbHiveLoader(LmdbHiveStore& store, const std::string& hiveName)
{
	std::map<std::string, std::string> records = store.readRecords(hiveName);

	if(records.empty())
	{
		throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "no hive named '" + hiveName + "' is stored");
	}

	std::vector<std::string> nodeOrder;
	std::vector<std::string> surfaceOrder;

	lmdbHiveRecords::parseMetaRecord(__requireRecord(records, lmdbHiveRecords::META_KEY), _hiveName, nodeOrder,
		surfaceOrder);

	if(_hiveName != hiveName)
	{
		throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "hive '" + hiveName + "' says it is named '" +
			_hiveName + "'");
	}

	for(const std::string& nodeName : nodeOrder)
	{
		std::string key = lmdbHiveRecords::nodeKey(nodeName);

		try
		{
			_nodes.push_back(JsonHiveLoader::parseNodeFragment(__requireRecord(records, key)));
		}
		catch(PersistException& exception)
		{
			if(exception.getError() == PersistException::LMDB_CORRUPT_RECORD) throw;

			throw __corrupt(key, exception);
		}

		// Records are found by the name in their key, so one that disagrees with it would be found under the
		// wrong name.
		if(_nodes.back().name != nodeName)
		{
			throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "'" + key + "' describes a node named '" +
				_nodes.back().name + "'");
		}
	}

	for(const std::string& surfaceName : surfaceOrder)
	{
		std::string key = lmdbHiveRecords::surfaceKey(surfaceName);

		try
		{
			_surfaces.push_back(JsonHiveLoader::parseSurfaceFragment(__requireRecord(records, key)));
		}
		catch(PersistException& exception)
		{
			if(exception.getError() == PersistException::LMDB_CORRUPT_RECORD) throw;

			throw __corrupt(key, exception);
		}

		if(_surfaces.back().name != surfaceName)
		{
			throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "'" + key + "' describes a surface named '" +
				_surfaces.back().name + "'");
		}
	}

	std::set<std::string> expectedKeys{lmdbHiveRecords::META_KEY};

	for(const std::string& nodeName : nodeOrder) expectedKeys.insert(lmdbHiveRecords::nodeKey(nodeName));
	for(const std::string& surfaceName : surfaceOrder) expectedKeys.insert(lmdbHiveRecords::surfaceKey(surfaceName));

	// Every other record must be one of the strobe registrations. A node or surface record the meta record does not
	// name would otherwise be silently dropped from the hive.
	for(const std::pair<const std::string, std::string>& record : records)
	{
		const std::string& key = record.first;

		try
		{
			if(lmdbHiveRecords::hasPrefix(key, lmdbHiveRecords::STROBE_EMITTER_KEY_PREFIX))
			{
				std::string nodeName;
				unsigned periodMs;

				JsonHiveLoader::parseStrobeEmitterFragment(record.second, nodeName, periodMs);

				_strobeEmitters.emplace_back(nodeName, periodMs);
			}
			else if(lmdbHiveRecords::hasPrefix(key, lmdbHiveRecords::STROBE_SURFACE_KEY_PREFIX))
			{
				std::string surfaceName;
				unsigned periodMs;

				JsonHiveLoader::parseStrobeSurfaceFragment(record.second, surfaceName, periodMs);

				_strobeSurfaces.emplace_back(surfaceName, periodMs);
			}
			else if(expectedKeys.find(key) == expectedKeys.end())
			{
				throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "'" + key + "' is not expected");
			}
		}
		catch(PersistException& exception)
		{
			if(exception.getError() == PersistException::LMDB_CORRUPT_RECORD) throw;

			throw __corrupt(key, exception);
		}
	}
}

LmdbHiveLoader::~LmdbHiveLoader()
{
}

std::string LmdbHiveLoader::getHiveName()
{
	return _hiveName;
}

unsigned LmdbHiveLoader::getNodeCount()
{
	return _nodes.size();
}

HiveNodeDescriptor LmdbHiveLoader::getNode(unsigned index)
{
	return _nodes.at(index);
}

unsigned LmdbHiveLoader::getSurfaceCount()
{
	return _surfaces.size();
}

HiveSurfaceDescriptor LmdbHiveLoader::getSurface(unsigned index)
{
	return _surfaces.at(index);
}

unsigned LmdbHiveLoader::getStrobeEmitterCount()
{
	return _strobeEmitters.size();
}

void LmdbHiveLoader::getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& periodMs)
{
	nodeName = _strobeEmitters.at(index).first;
	periodMs = _strobeEmitters.at(index).second;
}

unsigned LmdbHiveLoader::getStrobeSurfaceCount()
{
	return _strobeSurfaces.size();
}

void LmdbHiveLoader::getStrobeSurface(unsigned index, std::string& surfaceName, unsigned& periodMs)
{
	surfaceName = _strobeSurfaces.at(index).first;
	periodMs = _strobeSurfaces.at(index).second;
}
