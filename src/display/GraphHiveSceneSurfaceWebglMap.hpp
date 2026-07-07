#ifndef GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H
#define GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H

#include <string>

#include "GraphHiveSurfaceHttpMap.hpp"

class GraphHiveSceneSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Maps a GraphHiveSceneSurface onto a WebGL based HTML interface so that its geometry can be viewed in a browser.
 */
class GraphHiveSceneSurfaceWebglMap : public GraphHiveSurfaceHttpMap
{
    public:

        virtual ~GraphHiveSceneSurfaceWebglMap();

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Scene surface this map displays. Not owned by this.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSceneSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface, std::string path);

    protected:

        void _renderPage(HttpResponse& response) override;

        void _serveData(HttpRequest& request, HttpResponse& response) override;

    private:

        // Disable copying.
        GraphHiveSceneSurfaceWebglMap(const GraphHiveSceneSurfaceWebglMap& copyFrom);
        GraphHiveSceneSurfaceWebglMap& operator= (const GraphHiveSceneSurfaceWebglMap& copyFrom);

        /// Scene surface this map displays.
        GraphHiveSceneSurface& _sceneSurface;
};

#endif
