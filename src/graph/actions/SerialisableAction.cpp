#include "SerialisableAction.hpp"

#include "../GraphNode.hpp"
#include "../graphActionFlagRegister.hpp"
#include "../actionTargets/SerialisableActionTarget.hpp"
#include "SerialisableActionPayload.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

SerialisableAction::~SerialisableAction()
{
}

SerialisableAction::SerialisableAction(Handle<GraphNode>& initNode, unsigned energy,
	std::vector<unsigned> edgeTraversalKeys)
	: GraphAction(initNode, energy, 1, false, std::move(edgeTraversalKeys))
{
	// Many actions that are serialisable will need to still be invoked on non-serialising nodes.
	_addFlag(SERIALISABLE_GRAPH_ACTION, false);
}

bool SerialisableAction::_apply(GraphNode* target)
{
	SerialisableActionTarget* actionTarget = target -> getSerialisableActionTarget();

	if(actionTarget)
	{
		SerialisableActionPayload* payload = _serialise();

		if(payload)
		{
			try
			{
				__serialiseEdgeTraversalKeys(*payload);
			}
			catch(...)
			{
				payload -> decrRef();
				throw;
			}

			actionTarget -> send(*payload);
			payload -> decrRef();
		}
	}

	return false;
}

void SerialisableAction::_complete()
{
}

void SerialisableAction::deserialise(SerialisableActionPayload& data)
{
	_deserialise(data);
}

unsigned SerialisableAction::_getEdgeTraversalKeysSize()
{
	return static_cast<unsigned>(sizeof(uint32_t) * (_getEdgeTraversalKeys().size() + 1));
}

void SerialisableAction::__serialiseEdgeTraversalKeys(SerialisableActionPayload& payload)
{
	std::vector<unsigned> keys = _getEdgeTraversalKeys();

	for(unsigned key : keys)
	{
		payload.serialiseValue(static_cast<uint32_t>(key));
	}

	// The count goes last so that, reading back to front, it is the first thing deserialised.
	payload.serialiseValue(static_cast<uint32_t>(keys.size()));
}

std::vector<unsigned> SerialisableAction::deserialiseEdgeTraversalKeys(SerialisableActionPayload& data)
{
	uint32_t count;
	data.deserialiseValue(count);

	std::vector<unsigned> keys;

	for(uint32_t i = 0; i < count; i++)
	{
		uint32_t key;
		data.deserialiseValue(key);
		keys.push_back(key);
	}

	// The payload reads back to front, so the keys arrive last first.
	std::reverse(keys.begin(), keys.end());

	return keys;
}
