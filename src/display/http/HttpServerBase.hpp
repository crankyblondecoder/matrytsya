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

        /**
         * @param address IPv4 address of the interface to listen on, e.g. "127.0.0.1" for loopback only or
         *        "0.0.0.0" for every interface.
         * @param port Port to listen on.
         */
        HttpServerBase(std::string address, unsigned port);

        /**
         * Start the server listening on its configured address and port.
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
         * Serve over TLS (HTTPS) rather than plain HTTP.
         * @param certPath Path of the PEM file holding the server's certificate, or certificate chain.
         * @param keyPath Path of the PEM file holding the certificate's private key.
         * @note Must be called before start(). Both files are read here, once, so they may be made unreadable
         *       to this process afterwards.
         * @throw DisplayException If either file could not be read or is empty.
         */
        void setTls(std::string certPath, std::string keyPath);

        /**
         * Get whether this server serves over TLS.
         */
        bool getTlsEnabled();

        /**
         * Get the port this server is configured to listen on.
         */
        unsigned getPort();

        /**
         * Get the address this server is configured to listen on.
         */
        std::string getAddress();

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

        /**
         * Get the PEM certificate set by setTls(). Empty when TLS is not enabled.
         */
        const std::string& _getTlsCert();

        /**
         * Get the PEM private key set by setTls(). Empty when TLS is not enabled.
         */
        const std::string& _getTlsKey();

    private:

        // Disable copying.
        HttpServerBase(const HttpServerBase& copyFrom);
        HttpServerBase& operator= (const HttpServerBase& copyFrom);

        /// IPv4 address of the interface this server listens on.
        std::string _address;

        /// Port this server listens on.
        unsigned _port;

        /// Handlers registered against this server, keyed by path.
        std::map<std::string, HttpRequestHandler*> _handlers;

        /// Guards _handlers.
        ThreadMutex _lock;

        /// PEM certificate served over TLS. Empty when TLS is not enabled.
        std::string _tlsCert;

        /// PEM private key of the certificate served over TLS. Empty when TLS is not enabled.
        std::string _tlsKey;
};

#endif
