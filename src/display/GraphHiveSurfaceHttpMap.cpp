#include "GraphHiveSurfaceHttpMap.hpp"

#include "../thread/ThreadException.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpServerBase.hpp"

GraphHiveSurfaceHttpMap::GraphHiveSurfaceHttpMap(HttpServerBase& httpServer, GraphHiveSurface& surface,
	std::string path) :
	GraphHiveSurfaceMap(surface, path), _httpServer{httpServer}
{
	_httpServer.addHandler(getPath(), this);
}

GraphHiveSurfaceHttpMap::~GraphHiveSurfaceHttpMap()
{
	_httpServer.removeHandler(getPath());
}

void GraphHiveSurfaceHttpMap::handleRequest(HttpRequest& request, HttpResponse& response)
{
	__signalFirstRequest();

	std::string requestPath = request.getPath();
	std::string path = getPath();

	if(requestPath == path || requestPath == path + "/")
	{
		_renderPage(response);
	}
	else
	{
		_serveData(request, response);
	}
}

void GraphHiveSurfaceHttpMap::waitForFirstRequest(unsigned timeOut)
{
	try
	{
		_firstRequestCond.lockMutex();
	}
	catch(ThreadException& ex)
	{
		return;
	}

	if(!_receivedFirstRequest)
	{
		try
		{
			if(timeOut > 0)
			{
				unsigned loopLimit = 5;
				unsigned effTimeout = timeOut / loopLimit;
				if(effTimeout < 1) effTimeout = 1;

				while(!_receivedFirstRequest && loopLimit--) _firstRequestCond.waitTimeout(effTimeout);
			}
			else
			{
				while(!_receivedFirstRequest) _firstRequestCond.wait();
			}
		}
		catch(ThreadException& ex)
		{
			_firstRequestCond.unlockMutex();
			return;
		}
	}

	_firstRequestCond.unlockMutex();
}

bool GraphHiveSurfaceHttpMap::hasReceivedFirstRequest()
{
	bool received;

	_firstRequestCond.lockMutex();

	received = _receivedFirstRequest;

	_firstRequestCond.unlockMutex();

	return received;
}

void GraphHiveSurfaceHttpMap::__signalFirstRequest()
{
	_firstRequestCond.lockMutex();

	if(!_receivedFirstRequest)
	{
		_receivedFirstRequest = true;
		_firstRequestCond.broadcast();
	}

	_firstRequestCond.unlockMutex();
}
