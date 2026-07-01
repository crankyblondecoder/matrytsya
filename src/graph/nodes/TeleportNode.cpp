#include "TeleportNode.hpp"

#include "../GraphException.hpp"
#include "../GraphHive.hpp"
#include "../GraphHiveHandle.hpp"
#include "../graphActionFlagRegister.hpp"

TeleportNode::~TeleportNode()
{
}

TeleportNode::TeleportNode(GraphNodeLocation destination) : GraphNode(), _destination{destination}
{
	_setEnergyCost(1);

	// Supports serialisable action.
	_addActionFlag(SERIALISABLE_GRAPH_ACTION);
}

bool TeleportNode::send(SerialisableActionPayload& payload)
{
	bool okay = false;

	GraphHiveHandle hiveHandle = getHive();

	if(hiveHandle.isValid())
	{
		try
		{
			hiveHandle.getHive() -> teleportAction(payload, _destination);

			okay = true;
		}
		catch(GraphException& exception)
		{
			// Failure to teleport is not propagated; the action is simply lost.
			if(exception.getError() != GraphException::ACTION_TELEPORT_FAILED) throw;
		}
	}

	return okay;
}

SerialisableActionTarget* TeleportNode::getSerialisableActionTarget()
{
	return this;
}
