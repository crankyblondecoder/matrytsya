#include "StrobeEmitterNode.hpp"

#include "../actions/StrobeAction.hpp"
#include "../GraphHandle.hpp"

StrobeEmitterNode::~StrobeEmitterNode()
{
}

StrobeEmitterNode::StrobeEmitterNode() : GraphNode()
{
}

void StrobeEmitterNode::emitStrobe()
{
	GraphHandle<GraphNode> handle(this);

	// Action will self delete once complete.
	StrobeAction* action = new StrobeAction(handle);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

