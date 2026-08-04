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
		SCENE_SURFACE,
		GRAPH_VIEW_SURFACE
	};

	/// Concrete surface type this descriptor describes.
	Type type;

	/// Name that uniquely identifies this surface within its hive.
	std::string name;

	/// Name of the SceneRootNode, within the same hive, this surface is bound to. Only meaningful
	/// when type == SCENE_SURFACE.
	std::string sceneRootNodeName;

	/// Name of the node, within the same hive, the scene camera should initially focus on. Empty if
	/// not set. Only meaningful when type == SCENE_SURFACE.
	std::string initialFocusNodeName;

	/// Fraction of the viewport the focused node's bounds should span when the camera is first set
	/// up. Only meaningful when type == SCENE_SURFACE and initialFocusNodeName is set.
	double focusViewportFraction = 0.5;

	/// Whether this surface is the default surface of its kind within its hive.
	bool isDefault = false;
};

#endif
