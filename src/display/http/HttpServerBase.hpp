#ifndef HTTP_SERVER_BASE_H
#define HTTP_SERVER_BASE_H

#include <map>
#include <string>

#include "../../thread/ThreadMutex.hpp"

class HttpRequest;
class HttpResponse;
class HttpRequestHandler;

/**
 * Base class for a HTTP server that routes requests to registered handlers by path.
 * Concrete implementations provide the actual network transport.
 */
class HttpServerBase
{
    public:

        virtual ~HttpServerBase();

        HttpServerBase(unsigned port);

        /**
         * Start the server listening on its configured port.
         * @throw DisplayException If the server could not be started.
         */
        void start();

        /**
         * Stop the server.
         */
        void stop();

        /**
         * Register a handler for a path.
         * @note A registered path also matches any sub path, e.g. registering "/foo" also matches "/foo/bar".
         * @param path Path to route to the handler.
         * @param handler Handler to route to. Not owned by the server.
         * @throw DisplayException If a handler is already registered for the given path.
         */
        void addHandler(std::string path, HttpRequestHandler* handler);

        /**
         * Remove a previously registered handler.
         * @param path Path that was registered.
         */
        void removeHandler(std::string path);

        /**
         * Get the port this server is configured to listen on.
         */
        unsigned getPort();

    protected:

        /**
         * Sub-class hook to start the concrete server implementation.
         */
        virtual void _start() = 0;

        /**
         * Sub-class hook to stop the concrete server implementation.
         */
        virtual void _stop() = 0;

        /**
         * Route an incoming request to its registered handler, if any.
         * @param request Request to route.
         * @param response Response to populate. Left as a 404 if no handler matches.
         */
        void _routeRequest(HttpRequest& request, HttpResponse& response);

    private:

        // Disable copying.
        HttpServerBase(const HttpServerBase& copyFrom);
        HttpServerBase& operator= (const HttpServerBase& copyFrom);

        /// Port this server listens on.
        unsigned _port;

        /// Handlers registered against this server, keyed by path.
        std::map<std::string, HttpRequestHandler*> _handlers;

        /// Guards _handlers.
        ThreadMutex _lock;
};

#endif
