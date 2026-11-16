#ifndef JSON_HIVE_WRITER_H
#define JSON_HIVE_WRITER_H

#include <string>

struct HiveNodeDescriptor;
struct HiveSurfaceDescriptor;

/**
 * Writes the pieces of a hive definition out as JSON matching hiveSchema.json, one piece at a time, using
 * RapidJSON. The inverse of JsonHiveLoader's fragment parsers: what is written here reads back through them to
 * the descriptor it was written from.
 * @note Every field a descriptor's type gives meaning to is written, defaults included, so that what a stored
 *       hive means never depends on the defaults of whichever version reads it back.
 */
class JsonHiveWriter
{
	public:

		/**
		 * Write a node object, as it appears in the "nodes" array of a hive definition.
		 * @param descriptor Descriptor of the node to write.
		 * @returns JSON text of the node object.
		 * @throw PersistException(UNKNOWN_NODE_TYPE) If the descriptor's type has no name.
		 */
		static std::string nodeFragment(const HiveNodeDescriptor& descriptor);

		/**
		 * Write a surface object, as it appears in the "surfaces" array of a hive definition.
		 * @param descriptor Descriptor of the surface to write.
		 * @returns JSON text of the surface object.
		 * @throw PersistException(UNKNOWN_SURFACE_TYPE) If the descriptor's type has no name.
		 */
		static std::string surfaceFragment(const HiveSurfaceDescriptor& descriptor);

		/**
		 * Write a strobe emitter registration, as it appears in the "strobeEmitters" array of a hive definition.
		 * @param nodeName Name of the node registered as a strobe emitter.
		 * @param periodMs Emission period in milliseconds.
		 * @returns JSON text of the registration.
		 */
		static std::string strobeEmitterFragment(const std::string& nodeName, unsigned periodMs);

		/**
		 * Write a strobe surface registration, as it appears in the "strobeSurfaces" array of a hive definition.
		 * @param surfaceName Name of the surface registered as strobed.
		 * @param periodMs Strobe period in milliseconds.
		 * @returns JSON text of the registration.
		 */
		static std::string strobeSurfaceFragment(const std::string& surfaceName, unsigned periodMs);

	private:

		// Not instantiable.
		JsonHiveWriter();
		JsonHiveWriter(const JsonHiveWriter& copyFrom);
		JsonHiveWriter& operator= (const JsonHiveWriter& copyFrom);
};

#endif
