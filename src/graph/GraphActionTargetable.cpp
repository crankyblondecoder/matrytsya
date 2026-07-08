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

SceneStrobeActionTarget* GraphActionTargetable::getSceneStrobeActionTarget()
{
	return 0;
}

