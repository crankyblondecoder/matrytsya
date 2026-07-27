#include "SceneRootNode.hpp"

#include "../actions/SceneAction.hpp"
#include "../../util/Handle.hpp"
#include "../GraphHiveSceneSurface.hpp"

SceneRootNode::~SceneRootNode()
{
}

SceneRootNode::SceneRootNode() : StrobeEmitterNode()
{
	_setEnergyCost(1);
}

GraphNode::Type SceneRootNode::getType()
{
	return Type::SCENE_ROOT_NODE;
}

void SceneRootNode::populateSceneSurface(Handle<GraphHiveSceneSurface> sceneSurface)
{
	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	SceneAction* action = new SceneAction(handle, sceneSurface, 16535);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

void SceneRootNode::_poked(GraphPoke poke)
{
}
