#include "SceneGeometryNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneGeometryNode::~SceneGeometryNode()
{
}

SceneGeometryNode::SceneGeometryNode(const std::string& script) : ScriptNode(script)
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
}

void SceneGeometryNode::addVertexes(std::vector<Vertex> vertexesToAdd)
{
	_vertexes.insert(_vertexes.end(), vertexesToAdd.begin(), vertexesToAdd.end());
}

void SceneGeometryNode::populateSurface(GraphHiveSceneSurface& surface)
{
	surface.addVertexes(_vertexes);
}

SceneActionTarget* SceneGeometryNode::getSceneActionTarget()
{
	return this;
}

