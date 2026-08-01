#include "graphActionFlagRegister.hpp"
#include "GraphAction.hpp"

#include "GraphActionTargetable.hpp"

GraphActionTargetable::~GraphActionTargetable()
{
}

GraphActionTargetable::GraphActionTargetable()
{
}

void GraphActionTargetable::_addActionFlag(unsigned long actionFlag)
{
	_actionFlags |= actionFlag;
}

unsigned long GraphActionTargetable::getActionFlags()
{
	return _actionFlags;
}

bool GraphActionTargetable::canActionTarget(GraphAction* graphAction)
{
	// The actions required flags take precedence over its optional flags.
	unsigned long reqFlags = graphAction -> getRequiredFlags();

	if(reqFlags) return (reqFlags & _actionFlags) == reqFlags;

	return graphAction -> getOptionalFlags() & _actionFlags;
}

bool GraphActionTargetable::hasActionTarget(unsigned long actionFlag)
{
	return actionFlag & _actionFlags;
}

PingActionTarget* GraphActionTargetable::getPingActionTarget()
{
	return 0;
}

SerialisableActionTarget* GraphActionTargetable::getSerialisableActionTarget()
{
	return 0;
}

ScriptActionTarget* GraphActionTargetable::getScriptActionTarget()
{
	return 0;
}

SceneActionTarget* GraphActionTargetable::getSceneActionTarget()
{
	return 0;
}

StrobeActionTarget* GraphActionTargetable::getStrobeActionTarget()
{
	return 0;
}

AnimateActionTarget* GraphActionTargetable::getAnimateActionTarget()
{
	return 0;
}

VersionActionTarget* GraphActionTargetable::getVersionActionTarget()
{
	return 0;
}

AgentActionTarget* GraphActionTargetable::getAgentActionTarget()
{
	return 0;
}

TriggerActionTarget* GraphActionTargetable::getTriggerActionTarget()
{
	return 0;
}

AgentVisibleActionTarget* GraphActionTargetable::getAgentVisibleActionTarget()
{
	return 0;
}

