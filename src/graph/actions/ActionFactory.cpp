#include "ActionFactory.hpp"

#include "PingAction.hpp"
#include "SerialisableAction.hpp"
#include "SerialisableActionPayload.hpp"
#include "../GraphException.hpp"

SerialisableAction* ActionFactory::create(GraphNodeHandle& initNode, SerialisableActionPayload& payload)
{
	SerialisableAction* action = 0;

	switch(payload.getSerialisableActionType())
	{
		case SerialisableAction::SerialisableActionType::PING:
			action = new PingAction(initNode);
			break;

		default:
			throw GraphException(GraphException::UNKNOWN_ACTION_TYPE);
	}

	action -> deserialise(payload);

	return action;
}
