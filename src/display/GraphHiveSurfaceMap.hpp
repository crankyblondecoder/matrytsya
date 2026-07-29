#ifndef GRAPH_HIVE_SURFACE_MAP_H
#define GRAPH_HIVE_SURFACE_MAP_H

#include <string>

#include "../util/RefCounted.hpp"

class GraphHiveSurface;

/**
 * Transport agnostic base class for anything that maps a GraphHiveSurface onto an externally accessible interface.
 * @note Ref counted so that a listener binding to the surface it maps (see GraphHiveSceneSurfaceHtmlMap) can be
 *       guarded safely.
 */
class GraphHiveSurfaceMap : public RefCounted
{
    public:

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

        // Required by ref counting.
        virtual ~GraphHiveSurfaceMap();

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
