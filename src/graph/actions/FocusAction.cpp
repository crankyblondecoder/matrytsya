#include "FocusAction.hpp"

#include "../actionTargets/FocusActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

#include <utility>

FocusAction::~FocusAction()
{
}

FocusAction::FocusAction(Handle<GraphNode> initNode, std::string nodeName, double viewportFraction,
	std::vector<unsigned> edgeTraversalKeys)
	: GraphAction(initNode, _startingEnergy, 1, false, std::move(edgeTraversalKeys)), _nodeName(std::move(nodeName)),
	  _viewportFraction(viewportFraction)
{
	_addFlag(FOCUS_GRAPH_ACTION, true);
}

bool FocusAction::_apply(GraphNode* target)
{
	if(!_nodeName.empty() && target -> getName() != _nodeName) return false;

	FocusActionTarget* focusTarget = target -> getFocusActionTarget();

	if(focusTarget)
	{
		focusTarget -> requestFocus(_viewportFraction);
	}

	return false;
}

bool FocusAction::_starting()
{
	return true;
}

void FocusAction::_complete()
{
}
