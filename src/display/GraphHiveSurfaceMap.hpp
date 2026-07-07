#ifndef GRAPH_HIVE_SURFACE_MAP_H
#define GRAPH_HIVE_SURFACE_MAP_H

#include <string>

class GraphHiveSurface;

/**
 * Transport agnostic base class for anything that maps a GraphHiveSurface onto an externally accessible interface.
 */
class GraphHiveSurfaceMap
{
    public:

        virtual ~GraphHiveSurfaceMap();

        /**
         * @param surface Surface this map represents. Not owned by this.
         * @param path Path to mount this map's interface at, e.g. "/scene".
         */
        GraphHiveSurfaceMap(GraphHiveSurface& surface, std::string path);

        /**
         * Get the path this map is mounted at.
         */
        std::string getPath();

    protected:

        /**
         * Get the surface this map represents.
         */
        GraphHiveSurface& _getSurface();

    private:

        // Disable copying.
        GraphHiveSurfaceMap(const GraphHiveSurfaceMap& copyFrom);
        GraphHiveSurfaceMap& operator= (const GraphHiveSurfaceMap& copyFrom);

        /// Surface this map represents.
        GraphHiveSurface& _surface;

        /// Path this map is mounted at.
        std::string _path;
};

#endif
