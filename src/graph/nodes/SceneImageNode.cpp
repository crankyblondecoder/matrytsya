#include "SceneImageNode.hpp"

#include "../GraphNodeVisitor.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneImageNode::~SceneImageNode()
{
}

SceneImageNode::SceneImageNode()
	: GraphNode()
{
	_setEnergyCost(1);
	_addActionFlag(SCENE_GRAPH_ACTION);
	_addActionFlag(IMAGE_VISIBLE_GRAPH_ACTION);
	_addActionFlag(FOCUS_GRAPH_ACTION);
}

GraphNodeType SceneImageNode::getType()
{
	return GraphNodeType::SCENE_IMAGE_NODE;
}

void SceneImageNode::accept(GraphNodeVisitor& visitor)
{
	visitor.visit(*this);
}

void SceneImageNode::populateSurface(Handle<GraphHiveSceneSurface> surface)
{
	_populateFocusRequest(surface, getId());

	SceneImage::populateSurface(surface, getId());
}

SceneActionTarget* SceneImageNode::getSceneActionTarget()
{
	return this;
}

FocusActionTarget* SceneImageNode::getFocusActionTarget()
{
	return this;
}

ImageVisibleActionTarget* SceneImageNode::getImageVisibleActionTarget()
{
	return this;
}

void SceneImageNode::setImageVisible(bool visible)
{
	setVisible(visible);
}

unsigned SceneImageNode::getVersion()
{
	// A focus request is counted in so that one is enough on its own to make a scene action repopulate the
	// surface.
	return SceneImage::getVersion() + getFocusRequestCount();
}

void SceneImageNode::_poked(GraphPoke poke)
{
}
