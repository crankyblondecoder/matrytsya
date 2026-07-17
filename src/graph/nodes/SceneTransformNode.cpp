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

void SceneTransformNode::setTransform(const Transform transform)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];
}

void SceneTransformNode::populateSurface(GraphHandle<GraphHiveSceneSurface> surface)
{
	if(surface.isValid()) surface.getInstance() -> addLocalTransform(_transform, getId());
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
