#include "SceneTransformNode.hpp"
#include "../graphActionFlagRegister.hpp"

SceneTransformNode::~SceneTransformNode()
{
}

SceneTransformNode::SceneTransformNode() : GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
}

void SceneTransformNode::setTransform(Transform transform, bool isWorld)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];

	_transformAccumulates = isWorld;
}

void SceneTransformNode::populateSurface(GraphHiveSceneSurface& surface)
{
	// TODO ...
}

SceneActionTarget* SceneTransformNode::getSceneActionTarget()
{
	return this;
}
