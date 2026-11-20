#ifndef GRAPH_HIVE_SURFACE_HTTP_MAP_H
#define GRAPH_HIVE_SURFACE_HTTP_MAP_H

#include <atomic>
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

        bool retainHandler() final;

        void releaseHandler() final;

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

        /**
         * Stop serving the surface: unmount from the HTTP server and let go of anything the surface holds of
         * this, so that this is destroyed once the last handle to it is dropped.
         * @note Needed before dropping the last handle to a map whose hive keeps running or is about to be shut
         *       down, as a surface holds each map listening to it. Safe to call more than once.
         */
        void unmount();

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

        /**
         * Sub-class hook called by unmount(), to let go of anything the surface holds of this map.
         * @note Called once at most, before the map is unmounted from the server.
         */
        virtual void _unmounting();

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

        /// Whether this map is still mounted on the HTTP server.
        std::atomic<bool> _mounted{true};

        /// Set once the first HTTP request for this map has been received.
        bool _receivedFirstRequest = false;

        /// Signalled when the first HTTP request for this map is received.
        ThreadCondition _firstRequestCond;
};

#endif
