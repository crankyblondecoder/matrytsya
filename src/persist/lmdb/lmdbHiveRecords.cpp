#include "lmdbHiveRecords.hpp"

#include "../PersistException.hpp"
#include "../../rapidjson/document.h"
#include "../../rapidjson/stringbuffer.h"
#include "../../rapidjson/writer.h"

namespace
{
	void __writeNames(rapidjson::Writer<rapidjson::StringBuffer>& writer, const std::vector<std::string>& names)
	{
		writer.StartArray();

		for(const std::string& name : names) writer.String(name.c_str(), (rapidjson::SizeType) name.size());

		writer.EndArray();
	}

	void __readNames(const rapidjson::Value& meta, const char* key, std::vector<std::string>& names)
	{
		if(!meta.HasMember(key) || !meta[key].IsArray())
		{
			throw PersistException(PersistException::LMDB_CORRUPT_RECORD, std::string("meta is missing \"") + key + "\"");
		}

		names.clear();

		for(auto& nameValue : meta[key].GetArray())
		{
			if(!nameValue.IsString())
			{
				throw PersistException(PersistException::LMDB_CORRUPT_RECORD,
					std::string("meta \"") + key + "\" holds something other than a name");
			}

			names.push_back(std::string(nameValue.GetString(), nameValue.GetStringLength()));
		}
	}
}

const char* const lmdbHiveRecords::META_KEY = "meta";
const char* const lmdbHiveRecords::NODE_KEY_PREFIX = "node/";
const char* const lmdbHiveRecords::SURFACE_KEY_PREFIX = "surface/";
const char* const lmdbHiveRecords::STROBE_EMITTER_KEY_PREFIX = "strobeEmitter/";
const char* const lmdbHiveRecords::STROBE_SURFACE_KEY_PREFIX = "strobeSurface/";

std::string lmdbHiveRecords::nodeKey(const std::string& nodeName)
{
	return NODE_KEY_PREFIX + nodeName;
}

std::string lmdbHiveRecords::surfaceKey(const std::string& surfaceName)
{
	return SURFACE_KEY_PREFIX + surfaceName;
}

std::string lmdbHiveRecords::strobeEmitterKey(const std::string& nodeName)
{
	return STROBE_EMITTER_KEY_PREFIX + nodeName;
}

std::string lmdbHiveRecords::strobeSurfaceKey(const std::string& surfaceName)
{
	return STROBE_SURFACE_KEY_PREFIX + surfaceName;
}

bool lmdbHiveRecords::hasPrefix(const std::string& key, const char* prefix)
{
	return key.rfind(prefix, 0) == 0;
}

std::string lmdbHiveRecords::metaRecord(const std::string& hiveName, const std::vector<std::string>& nodeOrder,
	const std::vector<std::string>& surfaceOrder)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

	writer.StartObject();

	writer.Key("formatVersion");
	writer.Uint(FORMAT_VERSION);

	writer.Key("name");
	writer.String(hiveName.c_str(), (rapidjson::SizeType) hiveName.size());

	writer.Key("nodeOrder");
	__writeNames(writer, nodeOrder);

	writer.Key("surfaceOrder");
	__writeNames(writer, surfaceOrder);

	writer.EndObject();

	return std::string(buffer.GetString(), buffer.GetSize());
}

void lmdbHiveRecords::parseMetaRecord(const std::string& json, std::string& hiveName,
	std::vector<std::string>& nodeOrder, std::vector<std::string>& surfaceOrder)
{
	rapidjson::Document meta;

	meta.Parse(json.c_str(), json.size());

	if(meta.HasParseError() || !meta.IsObject())
	{
		throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "meta is not a JSON object");
	}

	if(!meta.HasMember("formatVersion") || !meta["formatVersion"].IsUint())
	{
		throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "meta is missing \"formatVersion\"");
	}

	// Checked before anything else is read, as another version need not hold anything else this one expects.
	if(meta["formatVersion"].GetUint() != FORMAT_VERSION)
	{
		throw PersistException(PersistException::LMDB_UNSUPPORTED_FORMAT,
			"version " + std::to_string(meta["formatVersion"].GetUint()));
	}

	if(!meta.HasMember("name") || !meta["name"].IsString())
	{
		throw PersistException(PersistException::LMDB_CORRUPT_RECORD, "meta is missing \"name\"");
	}

	hiveName = std::string(meta["name"].GetString(), meta["name"].GetStringLength());

	__readNames(meta, "nodeOrder", nodeOrder);
	__readNames(meta, "surfaceOrder", surfaceOrder);
}
