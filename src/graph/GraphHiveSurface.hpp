#ifndef GRAPH_HIVE_SURFACE_H
#define GRAPH_HIVE_SURFACE_H

#include "GraphNamed.hpp"

/**
 * Represents a "surface" that a hive can interact with, either for display or input.
 * It essentially provides a layer of abstraction for various display types but keeps the interface that an action
 * has to use consistent.
 */
class GraphHiveSurface : public GraphNamed
{
	public:

		GraphHiveSurface();

	protected:

		virtual ~GraphHiveSurface();

	private:

		// Disable copying.
		GraphHiveSurface(const GraphHiveSurface& copyFrom);
		GraphHiveSurface& operator= (const GraphHiveSurface& copyFrom);
};

#endif
