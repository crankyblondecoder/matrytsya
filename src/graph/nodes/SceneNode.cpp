#include "SceneNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneNode::~SceneNode()
{
}

SceneNode::SceneNode(const std::string& script) : ScriptNode(script)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
}

void SceneNode::addVertexes(std::vector<Vertex> vertexesToAdd)
{
	_vertexes.insert(_vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
}

void SceneNode::setTransform(Transform transform, bool isWorld)
{
	for(int i = 0; i < 16; i++) _transform[i] = transform[i];

	_transformAccumulates = isWorld;
}

void SceneNode::populateSurface(GraphHiveSceneSurface& surface)
{
	surface.addVertexes(_vertexes, _transform, _transformAccumulates);
}

SceneActionTarget* SceneNode::getSceneActionTarget()
{
	return this;
}

