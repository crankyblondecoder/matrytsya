#ifndef PING_NODE_H
#define PING_NODE_H

#include <atomic>

#include "../GraphSerialisedActionNode.hpp"
#include "../actionTargets/PingActionTarget.hpp"

class PingAction;

/**
 * Simple graph node that just provides a ping point.
 */
class PingNode : public GraphSerialisedActionNode, public PingActionTarget
{
    public:

        PingNode();

		GraphNodeType getType() override;

		bool ping() override;

		/**
		 * Get the number of times this node has been pinged.
		 * @note Safe to call while actions are still in flight, though the count it returns is only a snapshot
		 *       until they have completed.
		 */
		unsigned getPingCount();

		/**
		 * Emit a ping action from this node.
		 * @param wait Wait for action to complete.
		 * @returns Ping action that was emitted. Will be refincr so caller must decref this to dispose.
		 */
		PingAction* emitPing(bool wait);

		PingActionTarget* getPingActionTarget() override;

	protected:

		// Ref counted.
        virtual ~PingNode();

		void _poked(GraphPoke poke) override;

    private:

		/// Number of times this node has been pinged. Two actions can be applied to the same node on different
		/// worker threads at once, so this is incremented from several threads and must be atomic; a plain
		/// increment loses counts when two of them interleave.
		std::atomic<unsigned> _pingCount{0};
};

#endif
