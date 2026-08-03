#include "AgentAffectAction.hpp"

#include "../actionTargets/AgentAffectActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

AgentAffectAction::~AgentAffectAction()
{
}

AgentAffectAction::AgentAffectAction(Handle<GraphNode> initNode, bool agentAffecting)
	: GraphAction(initNode, _startingEnergy), _agentAffecting(agentAffecting)
{
	_addFlag(AGENT_AFFECT_GRAPH_ACTION, true);
}

bool AgentAffectAction::_apply(GraphNode* target)
{
	AgentAffectActionTarget* agentAffectTarget = target -> getAgentAffectActionTarget();

	if(agentAffectTarget)
	{
		if(_agentAffecting)
		{
			agentAffectTarget -> agentAffectingStart(false);
		}
		else
		{
			agentAffectTarget -> agentAffectingEnd(false);
		}
	}

	return false;
}

bool AgentAffectAction::_starting()
{
	return true;
}

void AgentAffectAction::_complete()
{
}
