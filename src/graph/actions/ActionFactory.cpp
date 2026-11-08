#include "ActionFactory.hpp"

#include "PingAction.hpp"
#include "SerialisableAction.hpp"
#include "SerialisableActionPayload.hpp"
#include "TriggerAction.hpp"
#include "../GraphException.hpp"

SerialisableAction* ActionFactory::create(Handle<GraphNode>& initNode, SerialisableActionPayload& payload)
{
	SerialisableAction* action = 0;

	// The type is checked before reading the keys, so an unknown type is reported as such rather than as
	// whatever reading its payload as keys happens to fail with.
	switch(payload.getSerialisableActionType())
	{
		case SerialisableAction::SerialisableActionType::PING:
		case SerialisableAction::SerialisableActionType::TRIGGER:
			break;

		default:
			throw GraphException(GraphException::UNKNOWN_ACTION_TYPE);
	}

	// Keys are fixed at construction and were serialised last, so they come off the payload first.
	std::vector<unsigned> edgeTraversalKeys = SerialisableAction::deserialiseEdgeTraversalKeys(payload);

	switch(payload.getSerialisableActionType())
	{
		case SerialisableAction::SerialisableActionType::PING:
			action = new PingAction(initNode, edgeTraversalKeys);
			break;

		case SerialisableAction::SerialisableActionType::TRIGGER:
			action = new TriggerAction(initNode, "", false, GraphNodeType::GRAPH_NODE, edgeTraversalKeys);
			break;

		default:
			throw GraphException(GraphException::UNKNOWN_ACTION_TYPE);
	}

	action -> deserialise(payload);

	return action;
}
