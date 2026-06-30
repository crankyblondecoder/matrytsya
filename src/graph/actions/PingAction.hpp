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

		PingAction(GraphNodeHandle& initNode);

		unsigned long getFlag() override;

		/**
		 * Get the current ping count from the action.
		 */
		unsigned getPingCount();

	protected:

		void _apply(GraphNode* target) override;

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
