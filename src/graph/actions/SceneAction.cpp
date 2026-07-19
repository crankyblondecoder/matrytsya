#include "SceneAction.hpp"

#include "../actionTargets/SceneActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphHiveSceneSurface.hpp"
#include "../GraphNode.hpp"

SceneAction::~SceneAction()
{
}

SceneAction::SceneAction(GraphHandle<GraphNode> initNode,GraphHandle<GraphHiveSceneSurface> surface)
	: GraphAction(initNode, _startingEnergy), _surface(surface)
{
	_addFlag(SCENE_GRAPH_ACTION, true);
}

void SceneAction::_apply(GraphNode* target)
{
	SceneActionTarget* sceneTarget = target -> getSceneActionTarget();

	if(sceneTarget)
	{
		sceneTarget -> populateSurface(_surface);
	}
}

bool SceneAction::_starting()
{
	if(_surface.isValid())
	{
		return _surface.getInstance() -> populateStart();
	}

	return false;
}

void SceneAction::_complete()
{
	if(_surface.isValid())
	{
		_surface.getInstance() -> populateEnd();
	}
}
