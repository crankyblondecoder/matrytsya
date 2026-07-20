#ifndef JSON_HIVE_LOADER_H
#define JSON_HIVE_LOADER_H

#include <string>
#include <utility>
#include <vector>

#include "../HiveLoader.hpp"
#include "../HiveNodeDescriptor.hpp"
#include "../HiveSurfaceDescriptor.hpp"

/**
 * HiveLoader that parses a JSON string matching hiveSchema.json, using RapidJSON.
 * @note This loads JSON that matches the schema; it does not validate arbitrary JSON against the
 *       schema. Unrecognised/extra fields are silently ignored rather than rejected.
 */
class JsonHiveLoader : public HiveLoader
{
	public:

		/**
		 * Parses and validates json immediately; all hive data is extracted up front.
		 * @param json JSON text describing a hive, per hiveSchema.json.
		 * @throw PersistException On any parse or structural validation failure.
		 */
        JsonHiveLoader(const std::string& json);

		virtual ~JsonHiveLoader();

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

        // Do not allow copying.
        JsonHiveLoader(const JsonHiveLoader& copyFrom);
        JsonHiveLoader& operator= (const JsonHiveLoader& copyFrom);

		/// Canonical name of the hive, parsed from the top level "name" member.
		std::string _hiveName;

		/// Descriptors of every node, parsed from the top level "nodes" array, in order.
		std::vector<HiveNodeDescriptor> _nodes;

		/// Descriptors of every surface, parsed from the top level "surfaces" array, in order.
		std::vector<HiveSurfaceDescriptor> _surfaces;

		/// Node name / periodMs pairs, parsed from the top level "strobeEmitters" array, in order.
		std::vector<std::pair<std::string, unsigned>> _strobeEmitters;

		/// Surface name / periodMs pairs, parsed from the top level "strobeSurfaces" array, in order.
		std::vector<std::pair<std::string, unsigned>> _strobeSurfaces;
};

#endif
