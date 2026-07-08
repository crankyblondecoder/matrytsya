#include "SceneStrobeAction.hpp"

#include "../actionTargets/SceneStrobeActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

SceneStrobeAction::~SceneStrobeAction()
{
}

SceneStrobeAction::SceneStrobeAction(GraphNodeHandle& initNode)
	: ScriptAction(initNode)
{
	_addFlag(SCENE_STROBE_GRAPH_ACTION, true);
}

void SceneStrobeAction::_apply(GraphNode* target)
{
	SceneStrobeActionTarget* strobeTarget = target -> getSceneStrobeActionTarget();

	if(strobeTarget)
	{
		strobeTarget -> strobe();
	}

	ScriptAction::_apply(target);
}
