#ifndef HIVE_SNAPSHOT_H
#define HIVE_SNAPSHOT_H

#include <string>
#include <vector>

#include "HiveNodeDescriptor.hpp"
#include "HiveSurfaceDescriptor.hpp"

/**
 * Describes a single strobe registration of a hive, of either a node or a surface.
 */
struct HiveStrobeDescriptor
{
	/// Name of the node or surface that is strobed.
	std::string name;

	/// Strobe period in milliseconds.
	unsigned periodMs = 0;
};

/**
 * Everything a running hive would have to be rebuilt from, as it stood at one moment.
 */
struct HiveSnapshot
{
	/// Name of the hive.
	std::string name;

	/// The hive's nodes, edges included, in the order they are to be rebuilt in.
	std::vector<HiveNodeDescriptor> nodes;

	/// The hive's surfaces, in the order they are to be rebuilt in.
	std::vector<HiveSurfaceDescriptor> surfaces;

	/// Nodes registered as strobe emitters.
	std::vector<HiveStrobeDescriptor> strobeEmitters;

	/// Surfaces registered to be strobed.
	std::vector<HiveStrobeDescriptor> strobeSurfaces;
};

#endif
