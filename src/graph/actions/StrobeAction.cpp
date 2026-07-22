#include "StrobeAction.hpp"

#include "../actionTargets/StrobeActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

StrobeAction::~StrobeAction()
{
}

StrobeAction::StrobeAction(Handle<GraphNode>& initNode)
	: ScriptAction(initNode)
{
	_addFlag(SCENE_STROBE_GRAPH_ACTION, true);
}

void StrobeAction::_apply(GraphNode* target)
{
	StrobeActionTarget* strobeTarget = target -> getStrobeActionTarget();

	if(strobeTarget)
	{
		strobeTarget -> setStrobe(true);
		strobeTarget -> strobe();
	}

	ScriptAction::_apply(target);

	if(strobeTarget)
	{
		// This is done after the script is applied so that the script can correctly query the strobing state.
		strobeTarget -> setStrobe(false);
	}
}

