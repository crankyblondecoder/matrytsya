#include "hiveNames.hpp"

#include "PersistException.hpp"
#include "../graph/graphActionFlagRegister.hpp"

#include <utility>

namespace
{
	const std::pair<const char*, HiveNodeDescriptor::Type> _NODE_TYPES[] =
	{
		{"PingNode", HiveNodeDescriptor::PING},
		{"TeleportNode", HiveNodeDescriptor::TELEPORT},
		{"SceneRootNode", HiveNodeDescriptor::SCENE_ROOT},
		{"SceneGeometryNode", HiveNodeDescriptor::SCENE_GEOMETRY},
		{"SceneGeometryScriptNode", HiveNodeDescriptor::SCENE_GEOMETRY_SCRIPT},
		{"SceneTransformNode", HiveNodeDescriptor::SCENE_TRANSFORM},
		{"SceneTransformScriptNode", HiveNodeDescriptor::SCENE_TRANSFORM_SCRIPT},
		{"AgentNode", HiveNodeDescriptor::AGENT},
		{"TriggerNode", HiveNodeDescriptor::TRIGGER},
		{"SceneTextScriptNode", HiveNodeDescriptor::SCENE_TEXT_SCRIPT},
		{"BuilderScriptNode", HiveNodeDescriptor::BUILDER_SCRIPT}
	};

	const std::pair<const char*, HiveSurfaceDescriptor::Type> _SURFACE_TYPES[] =
	{
		{"GraphHiveSceneSurface", HiveSurfaceDescriptor::SCENE_SURFACE},
		{"GraphHiveGraphViewSurface", HiveSurfaceDescriptor::GRAPH_VIEW_SURFACE}
	};

	// VERSION_GRAPH_ACTION is left out on purpose: no hive definition has ever been able to name it, so an edge
	// carrying it cannot have come from one.
	const std::pair<const char*, unsigned long> _ACTION_FLAGS[] =
	{
		{"PING_GRAPH_ACTION", PING_GRAPH_ACTION},
		{"SERIALISABLE_GRAPH_ACTION", SERIALISABLE_GRAPH_ACTION},
		{"SCRIPT_GRAPH_ACTION", SCRIPT_GRAPH_ACTION},
		{"SCENE_GRAPH_ACTION", SCENE_GRAPH_ACTION},
		{"SCENE_STROBE_GRAPH_ACTION", SCENE_STROBE_GRAPH_ACTION},
		{"ANIMATE_GRAPH_ACTION", ANIMATE_GRAPH_ACTION},
		{"AGENT_GRAPH_ACTION", AGENT_GRAPH_ACTION},
		{"TRIGGER_GRAPH_ACTION", TRIGGER_GRAPH_ACTION},
		{"AGENT_AFFECT_GRAPH_ACTION", AGENT_AFFECT_GRAPH_ACTION},
		{"TEXT_VISIBLE_GRAPH_ACTION", TEXT_VISIBLE_GRAPH_ACTION}
	};

	const std::pair<const char*, SceneGeometry::VertexVisibility> _VERTEX_VISIBILITIES[] =
	{
		{"ALWAYS", SceneGeometry::VertexVisibility::ALWAYS},
		{"AGENT", SceneGeometry::VertexVisibility::AGENT},
		{"GRABBED", SceneGeometry::VertexVisibility::GRABBED},
		{"DRAGGING", SceneGeometry::VertexVisibility::DRAGGING},
		{"HOVERED_OVER", SceneGeometry::VertexVisibility::HOVERED_OVER}
	};
}

HiveNodeDescriptor::Type hiveNames::nodeTypeFromName(const std::string& name)
{
	for(const std::pair<const char*, HiveNodeDescriptor::Type>& type : _NODE_TYPES)
	{
		if(name == type.first) return type.second;
	}

	throw PersistException(PersistException::UNKNOWN_NODE_TYPE, name);
}

std::string hiveNames::nodeTypeName(HiveNodeDescriptor::Type type)
{
	for(const std::pair<const char*, HiveNodeDescriptor::Type>& candidate : _NODE_TYPES)
	{
		if(type == candidate.second) return candidate.first;
	}

	throw PersistException(PersistException::UNKNOWN_NODE_TYPE);
}

HiveSurfaceDescriptor::Type hiveNames::surfaceTypeFromName(const std::string& name)
{
	for(const std::pair<const char*, HiveSurfaceDescriptor::Type>& type : _SURFACE_TYPES)
	{
		if(name == type.first) return type.second;
	}

	throw PersistException(PersistException::UNKNOWN_SURFACE_TYPE, name);
}

std::string hiveNames::surfaceTypeName(HiveSurfaceDescriptor::Type type)
{
	for(const std::pair<const char*, HiveSurfaceDescriptor::Type>& candidate : _SURFACE_TYPES)
	{
		if(type == candidate.second) return candidate.first;
	}

	throw PersistException(PersistException::UNKNOWN_SURFACE_TYPE);
}

unsigned long hiveNames::actionFlagFromName(const std::string& name)
{
	for(const std::pair<const char*, unsigned long>& flag : _ACTION_FLAGS)
	{
		if(name == flag.first) return flag.second;
	}

	throw PersistException(PersistException::UNKNOWN_ACTION_FLAG, name);
}

std::vector<std::string> hiveNames::actionFlagNames(unsigned long actionFlags)
{
	std::vector<std::string> names;

	unsigned long named = 0;

	for(const std::pair<const char*, unsigned long>& flag : _ACTION_FLAGS)
	{
		if(actionFlags & flag.second)
		{
			names.push_back(flag.first);
			named |= flag.second;
		}
	}

	if(actionFlags & ~named)
	{
		throw PersistException(PersistException::UNKNOWN_ACTION_FLAG, "bits " + std::to_string(actionFlags & ~named));
	}

	return names;
}

SceneGeometry::VertexVisibility hiveNames::vertexVisibilityFromName(const std::string& name)
{
	for(const std::pair<const char*, SceneGeometry::VertexVisibility>& visibility : _VERTEX_VISIBILITIES)
	{
		if(name == visibility.first) return visibility.second;
	}

	throw PersistException(PersistException::UNKNOWN_VERTEX_VISIBILITY, name);
}

std::string hiveNames::vertexVisibilityName(SceneGeometry::VertexVisibility visibility)
{
	for(const std::pair<const char*, SceneGeometry::VertexVisibility>& candidate : _VERTEX_VISIBILITIES)
	{
		if(visibility == candidate.second) return candidate.first;
	}

	throw PersistException(PersistException::UNKNOWN_VERTEX_VISIBILITY);
}
