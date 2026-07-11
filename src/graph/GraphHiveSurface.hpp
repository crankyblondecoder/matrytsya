#ifndef GRAPH_HIVE_SURFACE_H
#define GRAPH_HIVE_SURFACE_H

#include "../util/EventEmitter.hpp"
#include "../util/RefCounted.hpp"
#include "GraphHiveSurfaceListener.hpp"
#include "GraphNamed.hpp"

/**
 * Represents a "surface" that a hive can interact with, either for display or input.
 * It essentially provides a layer of abstraction for various display types but keeps the interface that an action
 * has to use consistent.
 */
class GraphHiveSurface : public RefCounted, public GraphNamed, public EventEmitter<GraphHiveSurfaceListener>
{
	public:

		GraphHiveSurface();

		/**
		 * Activate this surface.
		 * This must be done for the surface to start interacting with the hive.
		 */
		virtual void activate() = 0;

		/**
		 * Inform this surface that population of it has started.
		 */
		virtual void populateStart() = 0;

		/**
		 * Inform this surface that population of it has ended.
		 */
		virtual void populateEnd() = 0;

		/**
		 * Clean up and dereference this surface.
		 */
		virtual void close() final;

	protected:

		// Required by ref counting.
		virtual ~GraphHiveSurface();

		/**
		 * Subclass hook to indicate it should close.
		 */
		virtual void _close() = 0;

		/**
		 * Emit the surface changed event.
		 */
		void _emitSurfaceChanged();

	private:

		// Disable copying.
		GraphHiveSurface(const GraphHiveSurface& copyFrom);
		GraphHiveSurface& operator= (const GraphHiveSurface& copyFrom);
};

#endif
