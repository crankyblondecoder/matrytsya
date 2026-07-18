#ifndef HIVE_SURFACE_DESCRIPTOR_H
#define HIVE_SURFACE_DESCRIPTOR_H

#include <string>

/**
 * Format-agnostic description of a single hive surface, as supplied by a HiveLoader and consumed by
 * HiveBuilder. Fields not relevant to this descriptor's type are left at their defaults.
 */
struct HiveSurfaceDescriptor
{
	/// Identifies which concrete GraphHiveSurface subclass this descriptor describes.
	enum Type
	{
		SCENE_SURFACE
	};

	/// Concrete surface type this descriptor describes.
	Type type;

	/// Name that uniquely identifies this surface within its hive.
	std::string name;

	/// Name of the SceneRootNode, within the same hive, this surface is bound to. Only meaningful
	/// when type == SCENE_SURFACE.
	std::string sceneRootNodeName;
};

#endif
