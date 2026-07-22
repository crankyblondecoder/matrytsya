#include "AnimateAction.hpp"

#include "../actionTargets/AnimateActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

AnimateAction::~AnimateAction()
{
}

AnimateAction::AnimateAction(Handle<GraphNode> initNode, bool animating)
	: GraphAction(initNode, _startingEnergy), _animating(animating)
{
	_addFlag(ANIMATE_GRAPH_ACTION, true);
}

void AnimateAction::_apply(GraphNode* target)
{
	AnimateActionTarget* animateTarget = target -> getAnimateActionTarget();

	if(animateTarget)
	{
		animateTarget -> setAnimating(_animating, getId());
	}
}

bool AnimateAction::_starting()
{
	return true;
}

void AnimateAction::_complete()
{
}
