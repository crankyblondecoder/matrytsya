#ifndef GRAPH_SERIALISED_ACTION_NODE_H
#define GRAPH_SERIALISED_ACTION_NODE_H

#include <queue>

class GraphAction;

#include "GraphActionListener.hpp"
#include "GraphNode.hpp"
#include "../util/CastHandle.hpp"
#include "../util/EventListener.hpp"
#include "../util/Handle.hpp"

/**
 * Graph node that can emit its own graph actions.
 * @note Listens to the actions it emits via GraphActionListener, so that it can be given a hook into its own
 *       actions completing.
 * @note Can optionally serialise the actions it emits, see the serialiseEmittedActions constructor parameter.
 */
class GraphSerialisedActionNode : public GraphNode, private EventListener<GraphActionListener>
{
    public:

		/**
		 * @param serialiseEmittedActions If true, actions emitted by this node via _emitAction are queued and
		 *        only one is ever active at a time. The next queued action is started once the current one
		 *        emits its ACTION_COMPLETE event. Defaults to false, i.e. emitted actions run independently.
		 */
        GraphSerialisedActionNode(bool serialiseEmittedActions = false);

    protected:

		// Ref counted.
		virtual ~GraphSerialisedActionNode();

		/**
		 * Emit an action by making its origin this node.
		 * @note All subclasses must use this function to emit actions so that correct binding to the node occurs.
		 * @note If this node was constructed with serialiseEmittedActions true, the action is queued and only
		 *       started once every action emitted before it has completed.
		 * @param action Action to emit. This must have its refcount increased prior to the call.
		 */
		void _emitAction(GraphAction* action);

	private:

		// Do not allow copying.
		GraphSerialisedActionNode(const GraphSerialisedActionNode& copyFrom);
		GraphSerialisedActionNode& operator= (const GraphSerialisedActionNode& copyFrom);

		/// Generic lock.
		ThreadMutex _lock;

		/// Whether actions emitted via _emitAction are serialised, i.e. only one is active at a time.
		bool _serialiseActions;

		/// Queue of actions awaiting their turn to start, when _serialiseActions is true. The front entry is
		/// the currently active action.
		std::queue<Handle<GraphAction>> _actionQueue;

		virtual void populateEventListenerHandle(CastHandle<GraphActionListener>& handle) override;

		virtual void actionComplete(EventEmitter<GraphActionListener>& emitter) override;
};

#endif
