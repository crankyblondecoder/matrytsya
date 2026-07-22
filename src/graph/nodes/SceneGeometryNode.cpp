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

void SceneGeometryNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
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

void SceneGeometryNode::_poked(GraphPoke poke)
{
}

unsigned SceneGeometryNode::getVersion()
{
	return GraphVersioned::getVersion();
}
