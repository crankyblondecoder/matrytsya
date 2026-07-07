#include "SceneRootNode.hpp"

#include "../actions/SceneAction.hpp"
#include "../GraphException.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "../GraphNodeHandle.hpp"

SceneRootNode::~SceneRootNode()
{
}

SceneRootNode::SceneRootNode() : GraphNode()
{
	_setEnergyCost(1);
}

GraphHiveSceneSurface* SceneRootNode::generateSceneSurface(unsigned timeOut)
{
	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface();

	GraphNodeHandle handle(this);

	// Action will self delete once complete.
	SceneAction* action = new SceneAction(handle, *surface);

	action -> incrRef();

	_emitAction(action);

	action -> waitOnComplete(timeOut);

	bool completed = action -> isComplete();

	action -> decrRef();

	if(!completed)
	{
		// The action may still be traversing the graph and writing to the surface, so it can't be safely deleted
		// here: leaking it is preferable to a use-after-free once the action eventually does complete.
		throw GraphException(GraphException::SCENE_SURFACE_GENERATION_TIMED_OUT);
	}

	return surface;
}
