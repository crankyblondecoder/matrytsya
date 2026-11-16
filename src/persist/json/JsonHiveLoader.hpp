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

		/**
		 * Parse a single node object, as it appears in the "nodes" array of a hive definition.
		 * @param json JSON text of the node object.
		 * @returns Descriptor of the node.
		 * @throw PersistException On any parse or structural validation failure.
		 */
		static HiveNodeDescriptor parseNodeFragment(const std::string& json);

		/**
		 * Parse a single surface object, as it appears in the "surfaces" array of a hive definition.
		 * @param json JSON text of the surface object.
		 * @returns Descriptor of the surface.
		 * @throw PersistException On any parse or structural validation failure.
		 */
		static HiveSurfaceDescriptor parseSurfaceFragment(const std::string& json);

		/**
		 * Parse a single strobe emitter registration, as it appears in the "strobeEmitters" array of a hive
		 * definition.
		 * @param json JSON text of the registration.
		 * @param nodeName Set to the name of the node to register as a strobe emitter.
		 * @param periodMs Set to the emission period in milliseconds.
		 * @throw PersistException On any parse or structural validation failure.
		 */
		static void parseStrobeEmitterFragment(const std::string& json, std::string& nodeName, unsigned& periodMs);

		/**
		 * Parse a single strobe surface registration, as it appears in the "strobeSurfaces" array of a hive
		 * definition.
		 * @param json JSON text of the registration.
		 * @param surfaceName Set to the name of the surface to register as strobed.
		 * @param periodMs Set to the strobe period in milliseconds.
		 * @throw PersistException On any parse or structural validation failure.
		 */
		static void parseStrobeSurfaceFragment(const std::string& json, std::string& surfaceName, unsigned& periodMs);

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
