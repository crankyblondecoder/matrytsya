#ifndef GRAPH_HIVE_SCENE_SURFACE_HTML_MAP_H
#define GRAPH_HIVE_SCENE_SURFACE_HTML_MAP_H

#include <string>

#include "../util/Handle.hpp"
#include "GraphHiveSurfaceHtmlMap.hpp"

class GraphHiveSceneSurface;
class HttpServerBase;

/**
 * Maps a GraphHiveSceneSurface onto an HTML interface so that it can be viewed in a browser, in whatever way a
 * subclass chooses to draw it. Everything a browser needs of the bound surface whichever way it is drawn is held
 * by the base class; all this adds is the bound surface as the scene surface it is, which is what a subclass
 * needs to draw a scene rather than any other kind of surface from it.
 */
class GraphHiveSceneSurfaceHtmlMap : public GraphHiveSurfaceHtmlMap
{
    public:

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Scene surface this map binds to for its whole lifetime. This map keeps its own
         *        reference to it (see Handle), released once this map is destroyed.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSceneSurfaceHtmlMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface, std::string path);

    protected:

        // Required by ref counting.
        virtual ~GraphHiveSceneSurfaceHtmlMap();

        /**
         * Get the scene surface this map is bound to.
         */
        GraphHiveSceneSurface& _getSceneSurface();

    private:

        // Disable copying.
        GraphHiveSceneSurfaceHtmlMap(const GraphHiveSceneSurfaceHtmlMap& copyFrom);
        GraphHiveSceneSurfaceHtmlMap& operator= (const GraphHiveSceneSurfaceHtmlMap& copyFrom);

        /// Scene surface this map is bound to for its whole lifetime. Held as the surface it is, rather than
        /// reached through the base class, so that a subclass can ask it for the scene to draw.
        Handle<GraphHiveSceneSurface> _sceneSurface;
};

#endif
