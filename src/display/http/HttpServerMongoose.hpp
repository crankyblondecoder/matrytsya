#ifndef HTTP_SERVER_MONGOOSE_H
#define HTTP_SERVER_MONGOOSE_H

#include "HttpServerBase.hpp"

class HttpServerMongoosePollThread;
struct mg_mgr;
struct mg_connection;

/**
 * Mongoose based implementation of a HTTP server.
 * @note This is the only place in the display module that is aware of mongoose. Consumers should use it via the
 *       HttpServer typedef so that the underlying implementation remains swappable.
 */
class HttpServerMongoose : public HttpServerBase
{
    public:

        virtual ~HttpServerMongoose();

        HttpServerMongoose(unsigned port);

    protected:

        void _start() override;

        void _stop() override;

    private:

        // Disable copying.
        HttpServerMongoose(const HttpServerMongoose& copyFrom);
        HttpServerMongoose& operator= (const HttpServerMongoose& copyFrom);

        /**
         * Mongoose event handler callback.
         * @param connection Connection the event occurred on.
         * @param event Event type.
         * @param eventData Event specific data. A struct mg_http_message* when event is MG_EV_HTTP_MSG.
         */
        static void __eventHandler(mg_connection* connection, int event, void* eventData);

        /// Mongoose event manager.
        mg_mgr* _mgr;

        /// Thread that polls the mongoose event manager.
        HttpServerMongoosePollThread* _pollThread = 0;
};

#endif
