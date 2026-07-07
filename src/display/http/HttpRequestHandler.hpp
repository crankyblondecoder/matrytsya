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

    protected:

    private:
};

#endif
