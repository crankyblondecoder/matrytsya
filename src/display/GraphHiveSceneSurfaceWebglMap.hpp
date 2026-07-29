#ifndef GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H
#define GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H

#include <string>

#include "GraphHiveSceneSurfaceHtmlMap.hpp"

class GraphHiveSceneSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Maps a GraphHiveSceneSurface onto a WebGL based HTML interface so that its geometry can be viewed in a browser.
 * Everything the page needs of the bound surface that is not its geometry - the revision it polls, the pokes it
 * makes and the chat window it carries - comes from GraphHiveSceneSurfaceHtmlMap. What is added here is the WebGL
 * page itself and the scene data it draws from.
 */
class GraphHiveSceneSurfaceWebglMap : public GraphHiveSceneSurfaceHtmlMap
{
    public:

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Scene surface this map binds to for its whole lifetime. This map keeps its own
         *        reference to it (see Handle), released once this map is destroyed.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSceneSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface, std::string path);

    protected:

        // Required by ref counting.
        virtual ~GraphHiveSceneSurfaceWebglMap();

        void _renderPage(HttpResponse& response) override;

        /**
         * Serve the bound surface's scene as the JSON the WebGL page draws it from.
         * @param request The incoming data request. Its body may carry the viewer's list of the chunks it
         *        already holds vertex data for, as {"chunks":[{"nodeId":N,"chunkId":N,"vertexVersion":N}, ...]},
         *        so that only the vertexes it does not already have at the current version are sent. An empty or
         *        malformed body is treated as the viewer knowing nothing yet.
         * @param response Response to populate.
         */
        void _serveMapData(HttpRequest& request, HttpResponse& response) override;

    private:

        // Disable copying.
        GraphHiveSceneSurfaceWebglMap(const GraphHiveSceneSurfaceWebglMap& copyFrom);
        GraphHiveSceneSurfaceWebglMap& operator= (const GraphHiveSceneSurfaceWebglMap& copyFrom);
};

#endif
