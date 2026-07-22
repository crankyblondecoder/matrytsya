#include "StrobeEmitterNode.hpp"

#include "../actions/StrobeAction.hpp"
#include "../../util/Handle.hpp"

StrobeEmitterNode::~StrobeEmitterNode()
{
}

StrobeEmitterNode::StrobeEmitterNode() : GraphNode()
{
}

void StrobeEmitterNode::emitStrobe()
{
	Handle<GraphNode> handle(this);

	// Action will self delete once complete.
	StrobeAction* action = new StrobeAction(handle);

	action -> incrRef();

	_emitAction(action);

	action -> decrRef();
}

