#ifndef GRAPH_HIVE_SURFACE_HTTP_MAP_H
#define GRAPH_HIVE_SURFACE_HTTP_MAP_H

#include <string>

#include "GraphHiveSurfaceMap.hpp"
#include "http/HttpRequestHandler.hpp"

class GraphHiveSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Base class for anything that maps a GraphHiveSurface onto an HTTP accessible interface.
 * Handles registration with an HTTP server and routing between the interface's HTML page and its data endpoint.
 */
class GraphHiveSurfaceHttpMap : public GraphHiveSurfaceMap, public HttpRequestHandler
{
    public:

        virtual ~GraphHiveSurfaceHttpMap();

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Surface this map represents. Not owned by this.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSurfaceHttpMap(HttpServerBase& httpServer, GraphHiveSurface& surface, std::string path);

        void handleRequest(HttpRequest& request, HttpResponse& response) final;

    protected:

        /**
         * Render the HTML page for this map.
         * @param response Response to populate with the rendered page.
         */
        virtual void _renderPage(HttpResponse& response) = 0;

        /**
         * Serve a data request made by the rendered page, e.g. an XHR/fetch call.
         * @param request The incoming data request.
         * @param response Response to populate with the requested data.
         */
        virtual void _serveData(HttpRequest& request, HttpResponse& response) = 0;

    private:

        // Disable copying.
        GraphHiveSurfaceHttpMap(const GraphHiveSurfaceHttpMap& copyFrom);
        GraphHiveSurfaceHttpMap& operator= (const GraphHiveSurfaceHttpMap& copyFrom);

        /// HTTP server this map is registered with.
        HttpServerBase& _httpServer;
};

#endif
