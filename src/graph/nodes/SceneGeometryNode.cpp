#include "SceneGeometryNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "SceneGeometry.hpp"

SceneGeometryNode::~SceneGeometryNode()
{
}

SceneGeometryNode::SceneGeometryNode()
	: GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

GraphNode::Type SceneGeometryNode::getType()
{
	return Type::SCENE_GEOMETRY_NODE;
}

void SceneGeometryNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	SceneGeometry::populateSurface(surface, getId(), getPokeEnabled());
}

void SceneGeometryNode::strobe()
{
}

void SceneGeometryNode::setStrobe(bool flag)
{
	_strobe = flag;
}

SceneActionTarget* SceneGeometryNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneGeometryNode::getStrobeActionTarget()
{
	return this;
}

AgentVisibleActionTarget* SceneGeometryNode::getAgentVisibleActionTarget()
{
	return this;
}

void SceneGeometryNode::setAgentVisible(bool flag)
{
	SceneGeometry::setAgentVisible(flag);
}

bool SceneGeometryNode::getAgentVisible()
{
	return SceneGeometry::getAgentVisible();
}

void SceneGeometryNode::_poked(GraphPoke poke)
{
}

unsigned SceneGeometryNode::getVersion()
{
	// The scene version rather than the vertex version, so that a change to the agent visible flag alone is
	// still enough to make a scene action repopulate the surface.
	return SceneGeometry::getSceneVersion();
}
