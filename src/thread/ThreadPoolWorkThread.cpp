#include "ThreadPoolWorkThread.hpp"

#include <iostream>
#include "../log/log.hpp"
#include "ThreadException.hpp"
#include "ThreadPool.hpp"

ThreadPoolWorkThread::~ThreadPoolWorkThread()
{
	forceStop();

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

void ThreadPoolWorkThread::shutDown()
{
	_cond.lockMutex();
	_shutdown = true;

	// Worker thread might be waiting on condition.
	_cond.signal();
	_cond.unlockMutex();

	// Lock again so the thread active state can be read safely.
	_cond.lockMutex();

	// If worker thread is still active, wait on it to exit.
	if(_workerThreadActive)
	{
		// This should unlock mutex.
		_cond.wait();
	}

	_cond.unlockMutex();
}

bool ThreadPoolWorkThread::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	bool accepted = false;

	_cond.lockMutex();

	if(_canAcceptWorkUnit())
	{
		_curWorkUnit = workUnit;
		accepted = true;
		_cond.signal();
	}

	_cond.unlockMutex(); // This will trigger the condition to exit the wait() if it was signaled.

	return accepted;
}

void ThreadPoolWorkThread::threadEntry()
{
	// Any kind of thread exception simply terminates the work thread.

	try
	{
		// Use the conditions mutex as a lock to read the thread state because it is used to protect
		// work unit scheduling.
		_cond.lockMutex();

		_workerThreadActive = true;

		while(!getQuit() && !_shutdown)
		{
			if(_curWorkUnit)
			{
				_working = true;

				// Don't block for the entire processing of the work unit.
				_cond.unlockMutex();

				// Invoke the work load of the work unit.
				_curWorkUnit -> work();

				_cond.lockMutex();

				_working = false;

				// Work unit should be deleted at this point because it has reached the end of its life.
				delete _curWorkUnit;
				_curWorkUnit = 0;

				_cond.unlockMutex();

				// Finally tell the thread pool that a worker thread is free. This may or may not allocate a new work unit
				// to this thread.
				_threadPool -> workThreadFree();

				// Protects reading vars in while statement.
				_cond.lockMutex();
			}
			else
			{
				// Wait on the next unit of work.
				// The condition will unlock its mutex on wait.
				_cond.wait();

				// Conditions mutex will be locked on exit. Leave it that way to be able to safely read
				// the current work unit.
			}
		}

		_workerThreadActive = false;

		// Signal anything waiting on worker thread being inactive.
		_cond.broadcast();

		// Release anything waiting on worker thread exit.
		_cond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "threadPoolWorkThread exited unexpectedly:" + ex.getSubsystemErrorString() + "\n";
		LOG(Logger::LogLevel::DEBUG, msg)
	}

	// Make sure of this. Could have reached here because of exception.
	_workerThreadActive = false;
}

bool ThreadPoolWorkThread::canAcceptWorkUnit()
{
	_cond.lockMutex();

	bool canAccept = _canAcceptWorkUnit();

	_cond.unlockMutex();

	return canAccept;
}

bool ThreadPoolWorkThread::_canAcceptWorkUnit()
{
	return !getQuit() && !_shutdown && _workerThreadActive && _curWorkUnit == 0;
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