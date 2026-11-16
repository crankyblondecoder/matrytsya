#ifndef LMDB_HIVE_RECORDS_H
#define LMDB_HIVE_RECORDS_H

#include <string>
#include <vector>

/**
 * Layout of a hive's records in an LmdbHiveStore, shared by whatever writes a hive there and whatever reads one
 * back, so that the two cannot disagree about it.
 *
 * Each hive is a named database holding:
 * - "meta": the hive's name, the layout's format version, and the order its nodes and surfaces go in. Order is
 *   kept here because LMDB holds records in key order rather than the order they were written in.
 * - "node/<name>": one per node, a node object as it appears in a hive definition's "nodes" array.
 * - "surface/<name>": one per surface, a surface object as it appears in a hive definition's "surfaces" array.
 * - "strobeEmitter/<name>": one per strobe emitter registration, as it appears in "strobeEmitters".
 * - "strobeSurface/<name>": one per strobe surface registration, as it appears in "strobeSurfaces".
 */
namespace lmdbHiveRecords
{
	/// Key of the meta record.
	extern const char* const META_KEY;

	/// Prefix of the key of every node record.
	extern const char* const NODE_KEY_PREFIX;

	/// Prefix of the key of every surface record.
	extern const char* const SURFACE_KEY_PREFIX;

	/// Prefix of the key of every strobe emitter record.
	extern const char* const STROBE_EMITTER_KEY_PREFIX;

	/// Prefix of the key of every strobe surface record.
	extern const char* const STROBE_SURFACE_KEY_PREFIX;

	/// Version of the layout written. A reader refuses any other.
	const unsigned FORMAT_VERSION = 1;

	/**
	 * Get the key of a node's record.
	 * @param nodeName Name of the node.
	 */
	std::string nodeKey(const std::string& nodeName);

	/**
	 * Get the key of a surface's record.
	 * @param surfaceName Name of the surface.
	 */
	std::string surfaceKey(const std::string& surfaceName);

	/**
	 * Get the key of a strobe emitter registration's record.
	 * @param nodeName Name of the node registered.
	 */
	std::string strobeEmitterKey(const std::string& nodeName);

	/**
	 * Get the key of a strobe surface registration's record.
	 * @param surfaceName Name of the surface registered.
	 */
	std::string strobeSurfaceKey(const std::string& surfaceName);

	/**
	 * Get whether a key starts with a prefix.
	 * @param key Key to test.
	 * @param prefix Prefix to test for.
	 */
	bool hasPrefix(const std::string& key, const char* prefix);

	/**
	 * Write the meta record.
	 * @param hiveName Name of the hive.
	 * @param nodeOrder Names of the hive's nodes, in the order they are to be built in.
	 * @param surfaceOrder Names of the hive's surfaces, in the order they are to be built in.
	 * @returns JSON text of the meta record.
	 */
	std::string metaRecord(const std::string& hiveName, const std::vector<std::string>& nodeOrder,
		const std::vector<std::string>& surfaceOrder);

	/**
	 * Read the meta record.
	 * @param json JSON text of the meta record.
	 * @param hiveName Set to the name of the hive.
	 * @param nodeOrder Set to the names of the hive's nodes, in the order they are to be built in.
	 * @param surfaceOrder Set to the names of the hive's surfaces, in the order they are to be built in.
	 * @throw PersistException(LMDB_UNSUPPORTED_FORMAT) If the record was written in another format version.
	 * @throw PersistException(LMDB_CORRUPT_RECORD) If the record is malformed.
	 */
	void parseMetaRecord(const std::string& json, std::string& hiveName, std::vector<std::string>& nodeOrder,
		std::vector<std::string>& surfaceOrder);
}

#endif
