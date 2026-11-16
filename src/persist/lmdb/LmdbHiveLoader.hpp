#ifndef LMDB_HIVE_LOADER_H
#define LMDB_HIVE_LOADER_H

#include <string>
#include <utility>
#include <vector>

#include "../HiveLoader.hpp"
#include "../HiveNodeDescriptor.hpp"
#include "../HiveSurfaceDescriptor.hpp"

class LmdbHiveStore;

/**
 * HiveLoader that reads a hive back out of an LmdbHiveStore, from records laid out as lmdbHiveRecords.hpp
 * describes.
 */
class LmdbHiveLoader : public HiveLoader
{
	public:

		/**
		 * Reads and validates every record of the hive immediately; all hive data is extracted up front.
		 * @param store Store to read the hive from.
		 * @param hiveName Name of the hive to read.
		 * @throw PersistException(LMDB_CORRUPT_RECORD) If the hive is not held, or a record of it is missing or
		 *        malformed. The detail names the record.
		 * @throw PersistException(LMDB_UNSUPPORTED_FORMAT) If the hive was written in another format version.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be read.
		 */
		LmdbHiveLoader(LmdbHiveStore& store, const std::string& hiveName);

		virtual ~LmdbHiveLoader();

		std::string getHiveName() override;
		unsigned getNodeCount() override;
		HiveNodeDescriptor getNode(unsigned index) override;
		unsigned getSurfaceCount() override;
		HiveSurfaceDescriptor getSurface(unsigned index) override;
		unsigned getStrobeEmitterCount() override;
		void getStrobeEmitter(unsigned index, std::string& nodeName, unsigned& periodMs) override;
		unsigned getStrobeSurfaceCount() override;
		void getStrobeSurface(unsigned index, std::string& surfaceName, unsigned& periodMs) override;

	private:

		// Disable copying.
		LmdbHiveLoader(const LmdbHiveLoader& copyFrom);
		LmdbHiveLoader& operator= (const LmdbHiveLoader& copyFrom);

		/// Canonical name of the hive, read from its meta record.
		std::string _hiveName;

		/// Descriptors of every node, in the order the meta record gives.
		std::vector<HiveNodeDescriptor> _nodes;

		/// Descriptors of every surface, in the order the meta record gives.
		std::vector<HiveSurfaceDescriptor> _surfaces;

		/// Node name / periodMs pairs of every strobe emitter registration.
		std::vector<std::pair<std::string, unsigned>> _strobeEmitters;

		/// Surface name / periodMs pairs of every strobe surface registration.
		std::vector<std::pair<std::string, unsigned>> _strobeSurfaces;
};

#endif
