#include "VersionAction.hpp"

#include "../actionTargets/VersionActionTarget.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../GraphNode.hpp"

VersionAction::~VersionAction()
{
}

VersionAction::VersionAction(Handle<GraphNode> initNode)
	: GraphAction(initNode, _startingEnergy)
{
	_addFlag(VERSION_GRAPH_ACTION, true);
}

bool VersionAction::_apply(GraphNode* target)
{
	VersionActionTarget* versionTarget = target -> getVersionActionTarget();

	if(versionTarget)
	{
		_version += versionTarget -> getVersion();
	}

	return false;
}

bool VersionAction::_starting()
{
	return true;
}

void VersionAction::_complete()
{
}

unsigned VersionAction::getVersion()
{
	return _version;
}
