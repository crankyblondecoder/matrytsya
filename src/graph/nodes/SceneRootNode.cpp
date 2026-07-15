#include "SceneRootNode.hpp"

#include "../actions/SceneAction.hpp"
#include "../actions/StrobeAction.hpp"
#include "../GraphHandle.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneRootNode::~SceneRootNode()
{
}

SceneRootNode::SceneRootNode() : GraphNode()
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

void SceneRootNode::emitStrobe()
{
	GraphHandle<GraphNode> handle(this);

	// Action will self delete once complete.
	StrobeAction* action = new StrobeAction(handle);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

void SceneRootNode::_poked(GraphPoke poke)
{
}
