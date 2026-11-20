#ifndef HTTP_REQUEST_HANDLER_H
#define HTTP_REQUEST_HANDLER_H

class HttpRequest;
class HttpResponse;

/**
 * Interface for anything that wants to handle HTTP requests routed to it by a HttpServerBase.
 */
class HttpRequestHandler
{
    public:

        virtual ~HttpRequestHandler() {}

        HttpRequestHandler() {}

        /**
         * Handle an incoming HTTP request.
         * @param request The incoming request.
         * @param response Response to populate.
         */
        virtual void handleRequest(HttpRequest& request, HttpResponse& response) = 0;

        /**
         * Take a hold on this handler for the duration of one request, so that it cannot be destroyed part way
         * through answering it.
         * @returns False if this handler is already being destroyed, in which case it must not be called.
         * @note Called by the server under the lock that guards its handler table. A handler removes itself from
         *       that table before its memory is released, so it is always safe to call from there.
         */
        virtual bool retainHandler() = 0;

        /**
         * Release a hold taken by retainHandler().
         * @note Never called under a server lock, as releasing the last hold destroys the handler.
         */
        virtual void releaseHandler() = 0;

    protected:

    private:
};

#endif
