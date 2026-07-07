#ifndef HTTP_SERVER_MONGOOSE_POLL_THREAD_H
#define HTTP_SERVER_MONGOOSE_POLL_THREAD_H

#include "../../thread/Thread.hpp"

struct mg_mgr;

/**
 * Thread that repeatedly polls a mongoose event manager so that HttpServerMongoose can service connections.
 */
class HttpServerMongoosePollThread : public Thread
{
    public:

        virtual ~HttpServerMongoosePollThread();

        HttpServerMongoosePollThread(mg_mgr* mgr);

    protected:

        void threadEntry() override;

        void _quitRequested() override;

    private:

        // Disable copying.
        HttpServerMongoosePollThread(const HttpServerMongoosePollThread& copyFrom);
        HttpServerMongoosePollThread& operator= (const HttpServerMongoosePollThread& copyFrom);

        /// Mongoose event manager to poll. Not owned by this.
        mg_mgr* _mgr;
};

#endif
