#ifndef GRAPH_HIVE_STROBE_SCHEDULER_H
#define GRAPH_HIVE_STROBE_SCHEDULER_H

#include <time.h>

#include <vector>

#include "../thread/Thread.hpp"
#include "../thread/ThreadCondition.hpp"
#include "../util/Handle.hpp"

class StrobeEmitterNode;
class GraphHiveSurface;

/**
 * Dedicated thread owned by a GraphHive that drives per-node strobe emission.
 * Nodes inheriting StrobeEmitterNode can be registered with a period; this scheduler invokes
 * emitStrobe() on each registered node at its own cadence. GraphHiveSurface instances can similarly
 * be registered to have strobe() invoked at their own cadence.
 * @note Registration is external: the hive exposes methods that delegate here so that an external
 *       mechanism decides which nodes strobe. Nodes must be removed when they decouple from the hive.
 */
class GraphHiveStrobeScheduler : public Thread
{
    public:

        virtual ~GraphHiveStrobeScheduler();

        GraphHiveStrobeScheduler();

		/**
		 * Register a node as a strobe emitter, or update the period of an already registered node.
		 * @note If the node is not a StrobeEmitterNode, the handle is invalid or periodMs is 0, the
		 *       call is silently ignored.
		 * @note Once stop() has been called, registrations are silently ignored: the run loop is no
		 *       longer around to service (or release) them.
		 * @param node Handle of the node to register.
		 * @param periodMs Emission period in milliseconds (time between successive emissions).
		 */
		void setEmitter(Handle<StrobeEmitterNode> node, unsigned periodMs);

		/**
		 * Remove a node as a strobe emitter.
		 * @note Safe to call for a node that is not currently registered (no-op).
		 * @param node Handle of the node to remove.
		 */
		void removeEmitter(Handle<StrobeEmitterNode> node);

		/**
		 * Clear all emitters.
		 */
		void clearEmitters();

		/**
		 * Register a surface to be strobed, or update the period of an already registered surface.
		 * @note If the handle is invalid or periodMs is 0, the call is silently ignored.
		 * @note Once stop() has been called, registrations are silently ignored: the run loop is no
		 *       longer around to service (or release) them.
		 * @param surface Handle of the surface to register.
		 * @param periodMs Strobe period in milliseconds (time between successive strobes).
		 */
		void setSurface(Handle<GraphHiveSurface> surface, unsigned periodMs);

		/**
		 * Remove a surface from being strobed.
		 * @note Safe to call for a surface that is not currently registered (no-op).
		 * @param surface Handle of the surface to remove.
		 */
		void removeSurface(Handle<GraphHiveSurface> surface);

		/**
		 * Clear all registered surfaces.
		 */
		void clearSurfaces();

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
			Handle<StrobeEmitterNode> handle;

			/// Emission period in nanoseconds, derived from the registered millisecond period.
			long periodNs;

			/// Absolute CLOCK_MONOTONIC time of the next due emission.
			struct timespec nextDue;
		};

		/// A single registered strobed surface and its schedule.
		struct SurfaceEntry
		{
			/// Keeps the surface alive and provides identity for lookup/removal.
			Handle<GraphHiveSurface> handle;

			/// Strobe period in nanoseconds, derived from the registered millisecond period.
			long periodNs;

			/// Absolute CLOCK_MONOTONIC time of the next due strobe.
			struct timespec nextDue;
		};

		/// Registered emitters. Guarded by _cond's mutex.
		std::vector<StrobeEntry> _entries;

		/// Registered surfaces. Guarded by _cond's mutex.
		std::vector<SurfaceEntry> _surfaceEntries;

		/// Guards _entries, _surfaceEntries and provides the timed wait / wake mechanism for the run loop.
		ThreadCondition _cond;
};

#endif
