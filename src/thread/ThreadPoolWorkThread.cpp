#include "ThreadPoolWorkThread.hpp"

#include <iostream>

#include "ThreadPool.hpp"

ThreadPoolWorkThread::~ThreadPoolWorkThread()
{
	stop(true);

	if(_curWorkUnit) delete _curWorkUnit;
}

ThreadPoolWorkThread::ThreadPoolWorkThread(ThreadPool* threadPool)
{
	_workerThreadActive = false;
	_debugMarker = false;
	_curWorkUnit = 0;
	_threadPool = threadPool;
	_working = false;
	_shutdown = false;
}

bool ThreadPoolWorkThread::waitForReady(unsigned timeout)
{
	_workerThreadActiveCond.lockMutex();

	// This stops any chance of the while looping forever.
	unsigned loopLimit = 5;

	// This guarantees that the maximum time taken for this operation is timeout.
	unsigned effTimeout = timeout / loopLimit;

	while(!_shutdown && !_workerThreadActive && loopLimit--)
	{
		_workerThreadActiveCond.waitTimeout(effTimeout);
	}

	_workerThreadActiveCond.unlockMutex();

	return _workerThreadActive;
}

void ThreadPoolWorkThread::_quitRequested()
{
	// Just do nothing at this stage.
}

void ThreadPoolWorkThread::shutDown()
{
	_shutdown = true;

	// Worker thread might be waiting on current work unit condition.
	_curWorkUnitCond.lockMutex();
	_curWorkUnitCond.broadcast();
	_curWorkUnitCond.unlockMutex();

	// If worker thread is still active, wait on it to exit

	_workerThreadActiveCond.lockMutex();

	unsigned loopLimit = 5;

	while(_workerThreadActive && loopLimit--)
	{
		_workerThreadActiveCond.waitTimeout(2000);
	}

	_workerThreadActiveCond.unlockMutex();
}

bool ThreadPoolWorkThread::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	bool accepted = false;

	_curWorkUnitCond.lockMutex();

	if(__canAcceptWorkUnit())
	{
		accepted = true;
		_curWorkUnit = workUnit;
		_curWorkUnitCond.broadcast();
	}

	_curWorkUnitCond.unlockMutex();

	return accepted;
}

void ThreadPoolWorkThread::threadEntry()
{
	_workerThreadActiveCond.lockMutex();
	_workerThreadActive = true;
	_workerThreadActiveCond.broadcast();
	_workerThreadActiveCond.unlockMutex();

	while(!_getQuit() && !_shutdown)
	{
		_curWorkUnitCond.lockMutex();

		if(_curWorkUnit)
		{
			_curWorkUnitCond.unlockMutex();

			_working = true;

			try
			{
				// Invoke the work load of the work unit.
				_curWorkUnit -> work();
			}
			catch(...)
			{
				// Catch everything so that a bad work unit can't bring down the entire system.
			}

			_working = false;

			_curWorkUnitCond.lockMutex();

			// Work unit should be deleted at this point because it has reached the end of its life.
			delete _curWorkUnit;
			_curWorkUnit = 0;

			_curWorkUnitCond.unlockMutex();

			// Finally tell the thread pool that a worker thread is free. This may or may not allocate a new work unit
			// to this thread.
			_threadPool -> workerThreadFree();
		}
		else
		{
			// Wait on the next unit of work becoming available.
			if(!_shutdown) _curWorkUnitCond.wait();
			_curWorkUnitCond.unlockMutex();
		}
	}

	_workerThreadActiveCond.lockMutex();
	_workerThreadActive = false;
	_workerThreadActiveCond.broadcast();
	_workerThreadActiveCond.unlockMutex();
}

bool ThreadPoolWorkThread::canAcceptWorkUnit()
{
	_curWorkUnitCond.lockMutex();

	bool canAccept = __canAcceptWorkUnit();

	_curWorkUnitCond.unlockMutex();

	return canAccept;
}

bool ThreadPoolWorkThread::__canAcceptWorkUnit()
{
	return !_getQuit() && !_shutdown && _workerThreadActive && _curWorkUnit == 0;
}

void ThreadPoolWorkThread::enumerateState(unsigned numTabs)
{
	string indentTabs = "";
	for(unsigned level = 0; level < numTabs; level++)
	{
		indentTabs += "\t";
	}

	cout << indentTabs << "_debugMarker: " << _debugMarker << "\n";
	cout << indentTabs << "_workerThreadActive: " << _workerThreadActive << "\n";
	cout << indentTabs << "_working: " << _working << "\n";
	cout << indentTabs << "_shutdown: " << _shutdown << "\n";
}
