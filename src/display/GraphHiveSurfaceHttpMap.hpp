#ifndef GRAPH_HIVE_SURFACE_HTTP_MAP_H
#define GRAPH_HIVE_SURFACE_HTTP_MAP_H

#include <string>

#include "../thread/ThreadCondition.hpp"
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

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Surface this map represents. Not owned by this.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSurfaceHttpMap(HttpServerBase& httpServer, GraphHiveSurface& surface, std::string path);

        void handleRequest(HttpRequest& request, HttpResponse& response) final;

        /**
         * Block the calling thread until this map has received its first HTTP request, e.g. a browser loading
         * its page for the first time.
         * @param timeOut Maximum period in ms to wait. Use 0 to wait indefinitely.
         */
        void waitForFirstRequest(unsigned timeOut);

        /**
         * Get whether this map has received its first HTTP request yet.
         */
        bool hasReceivedFirstRequest();

    protected:

        // Required by ref counting.
        virtual ~GraphHiveSurfaceHttpMap();

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

        /**
         * Mark the first HTTP request as received, waking any thread blocked in waitForFirstRequest(), if this
         * is indeed the first request.
         */
        void __signalFirstRequest();

        /// HTTP server this map is registered with.
        HttpServerBase& _httpServer;

        /// Set once the first HTTP request for this map has been received.
        bool _receivedFirstRequest = false;

        /// Signalled when the first HTTP request for this map is received.
        ThreadCondition _firstRequestCond;
};

#endif
