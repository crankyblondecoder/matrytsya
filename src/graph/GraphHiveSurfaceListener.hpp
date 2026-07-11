#ifndef GRAPH_HIVE_SURFACE_LISTENER_H
#define GRAPH_HIVE_SURFACE_LISTENER_H

#include "GraphHandle.hpp"

class GraphHiveSurface;

/**
 * Provides ability to listen to graph hive surface events.
 */
class GraphHiveSurfaceListener
{
	public:

		GraphHiveSurfaceListener() {}

		virtual ~GraphHiveSurfaceListener() {}

		/**
		 * Indicate that the given hive surface has changed.
		 */
		virtual void hiveSurfaceChanged(GraphHandle<GraphHiveSurface> hiveSurface) = 0;

	protected:

	private:
};

#endif
