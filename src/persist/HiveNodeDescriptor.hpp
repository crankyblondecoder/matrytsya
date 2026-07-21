#ifndef HIVE_NODE_DESCRIPTOR_H
#define HIVE_NODE_DESCRIPTOR_H

#include <string>
#include <vector>

#include "../graph/GraphNodeLocation.hpp"
#include "../graph/graphSceneElements.hpp"

/**
 * Describes a single directed edge from a node being described by a HiveNodeDescriptor to another
 * node, by name, within the same hive.
 */
struct HiveEdgeDescriptor
{
	/// Name of the node, within the same hive, that this edge is directed to.
	std::string toNodeName;

	/// Names of the action flags that restrict which actions may traverse this edge, as they appear
	/// in graphActionFlagRegister.hpp (e.g. "PING_GRAPH_ACTION"). Empty means no restriction.
	std::vector<std::string> actionFlagNames;
};

/**
 * Format-agnostic description of a single hive node, as supplied by a HiveLoader and consumed by
 * HiveBuilder. Fields not relevant to this descriptor's type are left at their defaults.
 */
struct HiveNodeDescriptor
{
	/// Identifies which concrete GraphNode subclass this descriptor describes.
	enum Type
	{
		PING,
		TELEPORT,
		SCENE_ROOT,
		SCENE_GEOMETRY,
		SCENE_GEOMETRY_SCRIPT,
		SCENE_TRANSFORM,
		SCENE_TRANSFORM_SCRIPT
	};

	/// Concrete node type this descriptor describes.
	Type type;

	/// Name that uniquely identifies this node within its hive.
	std::string name;

	/// Whether poking is enabled for this node.
	bool pokeEnabled = false;

	/// Edges directed from this node to other nodes in the same hive.
	std::vector<HiveEdgeDescriptor> edges;

	/// Names of other nodes in the same hive that this node receives notifications from.
	std::vector<std::string> notifySourceNames;

	/// Destination of a TeleportNode. Only meaningful when type == TELEPORT.
	GraphNodeLocation destination;

	/// Vertexes of a SceneGeometryNode/SceneGeometryScriptNode. Only meaningful when hasVertexes is true.
	std::vector<Vertex> vertexes;

	/// Whether vertexes was supplied. Only meaningful when type == SCENE_GEOMETRY or SCENE_GEOMETRY_SCRIPT.
	bool hasVertexes = false;

	/// Transform of a SceneTransformNode/SceneTransformScriptNode. Only meaningful when hasTransform is true.
	Transform transform;

	/// Whether transform was supplied. Only meaningful when type == SCENE_TRANSFORM or SCENE_TRANSFORM_SCRIPT.
	bool hasTransform = false;

	/// Main Lua source a script node runs when invoked. Only meaningful for the *_SCRIPT types.
	std::string coreScript;

	/// Lua source a script node runs when poked. Only meaningful for the *_SCRIPT types.
	std::string pokeScript;
};

#endif
