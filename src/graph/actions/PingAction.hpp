#ifndef PING_ACTION_H
#define PING_ACTION_H

#include <atomic>

#include "SerialisableAction.hpp"
#include "SerialisableActionPayload.hpp"

/**
 * Ping graph action.
 */
class PingAction : public SerialisableAction
{
    public:

        virtual ~PingAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them.
		 */
		PingAction(Handle<GraphNode>& initNode, std::vector<unsigned> edgeTraversalKeys = {});

		/**
		 * Get the current ping count from the action.
		 */
		unsigned getPingCount();

		SerialisableActionType getSerialisbleType() override;

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

		SerialisableActionPayload* _serialise() override;

		void _deserialise(SerialisableActionPayload& data) override;

    private:

		/// Number of times this action has pinged a node.
		std::atomic<unsigned> _pingCount{0};

		/// Used for debugging only.
		unsigned _debugCount = 0;
};

#endif
