#include "TriggerAction.hpp"

#include "../actionTargets/TriggerActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

TriggerAction::~TriggerAction()
{
}

TriggerAction::TriggerAction(Handle<GraphNode> initNode, std::string nodeName, bool restrictToNodeType,
	GraphNode::Type nodeType)
	: GraphAction(initNode, _startingEnergy), _nodeName(nodeName), _restrictToNodeType(restrictToNodeType),
	  _nodeType(nodeType)
{
	_addFlag(TRIGGER_GRAPH_ACTION, true);
}

void TriggerAction::_apply(GraphNode* target)
{
	if(!_nodeName.empty() && target -> getName() != _nodeName) return;
	if(_restrictToNodeType && target -> getType() != _nodeType) return;

	TriggerActionTarget* triggerTarget = target -> getTriggerActionTarget();

	if(triggerTarget)
	{
		triggerTarget -> trigger();
	}
}

bool TriggerAction::_starting()
{
	return true;
}

void TriggerAction::_complete()
{
}
