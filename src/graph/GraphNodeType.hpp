#ifndef GRAPH_NODE_TYPE_H
#define GRAPH_NODE_TYPE_H

/// Identifies the concrete subclass of a node, so callers can find a specific kind without an RTTI cast.
enum class GraphNodeType
{
	/// A plain GraphNode. Reported by any node that doesn't identify a more specific type of its own.
	GRAPH_NODE,

	/// A PingNode.
	PING_NODE,

	/// A SceneGeometryNode.
	SCENE_GEOMETRY_NODE,

	/// A SceneTransformNode.
	SCENE_TRANSFORM_NODE,

	/// A ScriptNode.
	SCRIPT_NODE,

	/// A SceneGeometryScriptNode.
	SCENE_GEOMETRY_SCRIPT_NODE,

	/// A SceneTransformScriptNode.
	SCENE_TRANSFORM_SCRIPT_NODE,

	/// A SceneRootNode.
	SCENE_ROOT_NODE,

	/// A TeleportNode.
	TELEPORT_NODE,

	/// An AgentNode.
	AGENT_NODE,

	/// A TriggerNode.
	TRIGGER_NODE
};

#endif
