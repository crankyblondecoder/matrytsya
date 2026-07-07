#include "HttpServerMongoosePollThread.hpp"

#include "../../mongoose/mongoose.h"

HttpServerMongoosePollThread::HttpServerMongoosePollThread(mg_mgr* mgr) : _mgr{mgr}
{
}

HttpServerMongoosePollThread::~HttpServerMongoosePollThread()
{
}

void HttpServerMongoosePollThread::threadEntry()
{
	while(!_getQuit())
	{
		mg_mgr_poll(_mgr, 200);
	}
}

void HttpServerMongoosePollThread::_quitRequested()
{
	// Nothing to do. threadEntry() re-checks the quit flag every 200ms poll iteration.
}
