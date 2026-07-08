#include "SceneAction.hpp"

#include "../actionTargets/SceneActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

SceneAction::~SceneAction()
{
}

SceneAction::SceneAction(GraphNodeHandle& initNode, GraphHiveSceneSurface& surface)
	: GraphAction(initNode, 512), _surface(surface)
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

void SceneAction::_complete()
{
}
