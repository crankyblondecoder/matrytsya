#include "AgentAffectingActionEmitter.hpp"

AgentAffectingActionEmitter::~AgentAffectingActionEmitter()
{
}

AgentAffectingActionEmitter::AgentAffectingActionEmitter(bool emitsAgentAffectAction)
	: _emitsAgentAffectAction(emitsAgentAffectAction)
{
}

void AgentAffectingActionEmitter::_agentAffectingStart(bool direct)
{
	if(direct && _emitsAgentAffectAction)
	{
		_emitAgentAffectAction(true);
	}
}

void AgentAffectingActionEmitter::_agentAffectingEnd(bool direct)
{
	if(direct && _emitsAgentAffectAction)
	{
		_emitAgentAffectAction(false);
	}
}

