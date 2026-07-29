#include "GraphSerialisedActionNode.hpp"

#include "GraphAction.hpp"

GraphSerialisedActionNode::~GraphSerialisedActionNode()
{
}

GraphSerialisedActionNode::GraphSerialisedActionNode(bool serialiseEmittedActions)
	: GraphNode(), _serialiseActions(serialiseEmittedActions)
{
}

void GraphSerialisedActionNode::_emitAction(GraphAction* action)
{
	action -> addListener(*this);

	if(_serialiseActions)
	{
		bool startNow;

		{ SYNC(_lock)

			_actionQueue.push(Handle<GraphAction>(action));

			// Only the first entry pushed into an empty queue is started immediately; anything else is
			// started later, from actionComplete, once it reaches the front.
			startNow = (_actionQueue.size() == 1);
		}

		if(startNow) action -> start();
	}
	else
	{
		action -> start();
	}
}

void GraphSerialisedActionNode::actionComplete(EventEmitter<GraphActionListener>& emitter)
{
	emitter.removeListener(*this);

	if(_serialiseActions)
	{
		Handle<GraphAction> nextAction(0);

		{ SYNC(_lock)

			// The completed action is always the current front of the queue.
			if(!_actionQueue.empty()) _actionQueue.pop();

			if(!_actionQueue.empty()) nextAction = _actionQueue.front();
		}

		// Started outside of the lock because this can re-enter this node, eg via synchronous completion.
		if(nextAction.isValid()) nextAction.getInstance() -> start();
	}
}

void GraphSerialisedActionNode::populateEventListenerHandle(CastHandle<GraphActionListener>& handle)
{
	handle = CastHandle<GraphActionListener>(this, this);
}
