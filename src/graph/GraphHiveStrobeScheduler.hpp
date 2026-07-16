#ifndef GRAPH_HIVE_STROBE_SCHEDULER_H
#define GRAPH_HIVE_STROBE_SCHEDULER_H

#include <time.h>

#include <vector>

#include "../thread/Thread.hpp"
#include "../thread/ThreadCondition.hpp"
#include "GraphHandle.hpp"

class GraphNode;
class StrobeEmitterNode;

/**
 * Dedicated thread owned by a GraphHive that drives per-node strobe emission.
 * Nodes inheriting StrobeEmitterNode can be registered with a frequency; this scheduler invokes
 * emitStrobe() on each registered node at its own cadence.
 * @note Registration is external: the hive exposes methods that delegate here so that an external
 *       mechanism decides which nodes strobe. Nodes must be removed when they decouple from the hive.
 */
class GraphHiveStrobeScheduler : public Thread
{
    public:

        virtual ~GraphHiveStrobeScheduler();

        GraphHiveStrobeScheduler();

		/**
		 * Register a node as a strobe emitter, or update the frequency of an already registered node.
		 * @note If the node is not a StrobeEmitterNode, the handle is invalid or frequencyHz is 0, the
		 *       call is silently ignored.
		 * @param node Handle of the node to register.
		 * @param frequencyHz Emission frequency in Hz (emissions per second).
		 */
		void setEmitter(GraphHandle<GraphNode> node, unsigned frequencyHz);

		/**
		 * Remove a node as a strobe emitter.
		 * @note Safe to call for a node that is not currently registered (no-op).
		 * @param node Node to remove. Identity is by pointer.
		 */
		void removeEmitter(GraphNode* node);

    protected:

		void threadEntry() override;

		void _quitRequested() override;

    private:

        // Do not allow copying.
        GraphHiveStrobeScheduler(const GraphHiveStrobeScheduler& copyFrom);
        GraphHiveStrobeScheduler& operator= (const GraphHiveStrobeScheduler& copyFrom);

		/// A single registered strobe emitter and its schedule.
		struct StrobeEntry
		{
			/// Keeps the node alive and provides identity for lookup/removal.
			GraphHandle<GraphNode> handle;

			/// Cached emitter view of the same object (validated dynamic_cast of handle).
			StrobeEmitterNode* emitter;

			/// Emission period in nanoseconds, derived from frequency.
			long periodNs;

			/// Absolute CLOCK_MONOTONIC time of the next due emission.
			struct timespec nextDue;
		};

		/// Registered emitters. Guarded by _cond's mutex.
		std::vector<StrobeEntry> _entries;

		/// Guards _entries and provides the timed wait / wake mechanism for the run loop.
		ThreadCondition _cond;
};

#endif
