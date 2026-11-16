#ifndef HIVE_NAMES_H
#define HIVE_NAMES_H

#include <string>
#include <vector>

#include "HiveNodeDescriptor.hpp"
#include "HiveSurfaceDescriptor.hpp"
#include "../graph/nodes/SceneGeometry.hpp"

/**
 * Translation between the names a persisted hive uses for enumerated values and the values themselves, in both
 * directions, so that whatever reads a hive and whatever writes one back out cannot disagree about them.
 */
namespace hiveNames
{
	/**
	 * Translate a node type name into the node type it stands for.
	 * @param name Node type name, as it appears in a hive definition (e.g. "PingNode").
	 * @returns The node type.
	 * @throw PersistException(UNKNOWN_NODE_TYPE) If the name is not recognised.
	 */
	HiveNodeDescriptor::Type nodeTypeFromName(const std::string& name);

	/**
	 * Translate a node type into its name.
	 * @param type Node type to translate.
	 * @returns The name the type has in a hive definition.
	 * @throw PersistException(UNKNOWN_NODE_TYPE) If the type has no name.
	 */
	std::string nodeTypeName(HiveNodeDescriptor::Type type);

	/**
	 * Translate a surface type name into the surface type it stands for.
	 * @param name Surface type name, as it appears in a hive definition (e.g. "GraphHiveSceneSurface").
	 * @returns The surface type.
	 * @throw PersistException(UNKNOWN_SURFACE_TYPE) If the name is not recognised.
	 */
	HiveSurfaceDescriptor::Type surfaceTypeFromName(const std::string& name);

	/**
	 * Translate a surface type into its name.
	 * @param type Surface type to translate.
	 * @returns The name the type has in a hive definition.
	 * @throw PersistException(UNKNOWN_SURFACE_TYPE) If the type has no name.
	 */
	std::string surfaceTypeName(HiveSurfaceDescriptor::Type type);

	/**
	 * Translate an action flag name into its bit value.
	 * @param name Action flag name, as it appears in graphActionFlagRegister.hpp.
	 * @returns The flag's bit value.
	 * @throw PersistException(UNKNOWN_ACTION_FLAG) If the name is not recognised.
	 */
	unsigned long actionFlagFromName(const std::string& name);

	/**
	 * Translate an action flags bit field into the names of the flags set in it.
	 * @param actionFlags Bit field to translate.
	 * @returns Name of every flag set, in bit order.
	 * @throw PersistException(UNKNOWN_ACTION_FLAG) If a bit is set that has no name a hive can be loaded with.
	 */
	std::vector<std::string> actionFlagNames(unsigned long actionFlags);

	/**
	 * Translate a vertex visibility name into its enum value.
	 * @param name Vertex visibility name, as it appears in SceneGeometry::VertexVisibility.
	 * @returns The visibility.
	 * @throw PersistException(UNKNOWN_VERTEX_VISIBILITY) If the name is not recognised.
	 */
	SceneGeometry::VertexVisibility vertexVisibilityFromName(const std::string& name);

	/**
	 * Translate a vertex visibility into its name.
	 * @param visibility Visibility to translate.
	 * @returns The visibility's name.
	 * @throw PersistException(UNKNOWN_VERTEX_VISIBILITY) If the visibility has no name.
	 */
	std::string vertexVisibilityName(SceneGeometry::VertexVisibility visibility);
}

#endif
