#include "SceneTransformNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneTransformNode::~SceneTransformNode()
{
}

SceneTransformNode::SceneTransformNode()
	: GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

GraphNode::Type SceneTransformNode::getType()
{
	return Type::SCENE_TRANSFORM_NODE;
}

void SceneTransformNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	SceneTransform::populateSurface(surface, getId());
}

void SceneTransformNode::strobe()
{
}

void SceneTransformNode::setStrobe(bool flag)
{
	_strobe = flag;
}

SceneActionTarget* SceneTransformNode::getSceneActionTarget()
{
	return this;
}

StrobeActionTarget* SceneTransformNode::getStrobeActionTarget()
{
	return this;
}

void SceneTransformNode::_poked(GraphPoke poke)
{
}

unsigned SceneTransformNode::getVersion()
{
	return GraphVersioned::getVersion();
}
