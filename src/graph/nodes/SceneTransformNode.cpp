#include "SceneTransformNode.hpp"

#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneTransformNode::~SceneTransformNode()
{
}

SceneTransformNode::SceneTransformNode() : GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(SCENE_STROBE_GRAPH_ACTION);
}

void SceneTransformNode::setTransform(Transform transform)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];
}

void SceneTransformNode::populateSurface(GraphHiveSceneSurface& surface)
{
	surface.addLocalTransform(_transform, getId());
}

void SceneTransformNode::strobe()
{
}

SceneActionTarget* SceneTransformNode::getSceneActionTarget()
{
	return this;
}

SceneStrobeActionTarget* SceneTransformNode::getSceneStrobeActionTarget()
{
	return this;
}
