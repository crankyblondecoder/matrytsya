#include "SceneRootNode.hpp"

#include "../actions/SceneAction.hpp"
#include "../GraphHandle.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneRootNode::~SceneRootNode()
{
}

SceneRootNode::SceneRootNode() : StrobeEmitterNode()
{
	_setEnergyCost(1);
}

void SceneRootNode::populateSceneSurface(GraphHandle<GraphHiveSceneSurface> sceneSurface)
{
	GraphHandle<GraphNode> handle(this);

	// Action will self delete once complete.
	SceneAction* action = new SceneAction(handle, sceneSurface);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

void SceneRootNode::_poked(GraphPoke poke)
{
}

void SceneRootNode::notify(NotifyType type)
{
	if(type == NotifyType::SCENE_DATA_CHANGED) _sceneVersion++;
}

unsigned SceneRootNode::getSceneVersion()
{
	return _sceneVersion;
}
