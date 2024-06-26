#include "ThreadPool.hpp"

#include <iostream>
#include "../log/log.hpp"
#include <new>
#include "ThreadException.hpp"
#include "ThreadPoolWorkThread.hpp"

using namespace std;

ThreadPool* threadPool = 0;

// TODO How can threads be pre-empted if they run for too long?
//      Maybe there is a way to downgrade a threads priority while it is running?
//      Maybe there is a way to kill a thread from outside the thread?
//      Needs to be some way of notifying work units so that any associated resources can be released.

void startThreadPool(unsigned numThreads)
{
	if(!threadPool)
	{
		threadPool = new ThreadPool(numThreads);
	}
}

void stopThreadPool()
{
	if(threadPool)
	{
		threadPool -> shutdown();

		delete threadPool;
	}
}

void enumerateThreadPool(unsigned numTabs)
{
	if(threadPool)
	{
		threadPool -> enumerateState(numTabs);
	}
}

ThreadPool::~ThreadPool()
{
	unsigned index;

	// The pool thread must be stopped first.
	// Ideally this should have been done gracefully way before destruction.
	forceStop();

	// Force the worker threads to stop.
	// Again, ideally that should have already been done gracefully.
	// Delete once stopped.
	for(index = 0; index < _numThreads; index++)
	{
		if(_pool[index])
		{
			_pool[index] -> forceStop();
			delete _pool[index];
		}
	}

	// Deallocate everything else.

	delete[] _pool;
}

ThreadPool::ThreadPool(unsigned numThreads)
{
	_debugMarker = false;
	_workUnitAllocPassInProgress = false;
	_viableWorkerThreadCount = 0;
	_numWorkerThreadsFree = 0;
	_numThreads = numThreads;
	_poolThreadActive = false;
	_shutdown = false;

	try
	{
		_pool = new ThreadPoolWorkThread*[numThreads];
	}
	catch(std::bad_alloc& ex)
	{
		throw ThreadException(ThreadException::THREAD_POOL_BAD_ALLOC);
	}

	unsigned index;

	// Try and create all worker threads at once.

	for(index = 0; index < numThreads; index++)
	{
		try
		{
			_pool[index] = new ThreadPoolWorkThread(this);
		}
		catch(std::bad_alloc& ex)
		{
			// Unwind already allocated workers.

			for(unsigned unwindIndex = 0; unwindIndex < index; unwindIndex++)
			{
				delete _pool[unwindIndex];
			}

			delete[] _pool;

			throw ThreadException(ThreadException::THREAD_POOL_CANT_ALLOC_WORKER);
		}
	}

	// If allocation of threads is successful. Try and start them.
	// This is allowed to partially succeed.

	for(index = 0; index < numThreads; index++)
	{
		try
		{
			_pool[index] -> start();
			_viableWorkerThreadCount++;
		}
		catch(ThreadException& ex)
		{
			delete _pool[index];
			_pool[index] = 0;
		}
	}

	if(!_viableWorkerThreadCount)
	{
		throw ThreadException(ThreadException::THREAD_POOL_COULD_NOT_START_ANY_WORKERS, 0);
	}
	else
	{
		_numWorkerThreadsFree = _viableWorkerThreadCount;

		// Start the queue thread.
		start();
	}
}

void ThreadPool::threadEntry()
{
	unsigned nextAllocThreadIndex;
	bool allocated;
	unsigned numAllocTrys;
	ThreadPoolWorkThread* workerThread;

	try
	{
		_queueCond.lockMutex();

		_poolThreadActive = true;

		while(!getQuit() && !_shutdown)
		{
			// Assume queue mutex is locked on entry into this loop.
			// Number worker threads free needs to be protected on read.

			if(_numWorkerThreadsFree)
			{
				ThreadPoolWorkUnit* workUnit = 0;

				if(_workUnitQueue.count())
				{
					workUnit = _workUnitQueue.first();
					_workUnitQueue.remove();
				}

				if(workUnit)
				{
					// Release the queue lock so that work units can be added to the queue without unnecessary blocking.
					_queueCond.unlockMutex();

					// Allocate work unit to thread in round robin fashion.

					numAllocTrys = 0;
					allocated = false;
					nextAllocThreadIndex = _lastAllocThreadIndex + 1;

					while(!_shutdown && !allocated && numAllocTrys < _numThreads)
					{
						if(nextAllocThreadIndex >= _numThreads) nextAllocThreadIndex = 0;

						_queueCond.lockMutex();
						workerThread = _pool[nextAllocThreadIndex];
						_queueCond.unlockMutex();

						if(!_shutdown && workerThread)
						{
							allocated = workerThread -> executeWorkUnit(workUnit);
						}

						if(!allocated)
						{
							numAllocTrys++;
							nextAllocThreadIndex++;
						}
						else
						{
							_lastAllocThreadIndex = nextAllocThreadIndex;
						}
					}

					// The rest of the loop expects the queue to be locked.
					// Placed here to protect write of num worker threads free.
					_queueCond.lockMutex();

					if(!allocated)
					{
						std::string msg = "ThreadPool was not able to allocate work unit to worker thread. This was unexpected.\n";
						LOG(Logger::LogLevel::ERROR, msg);
					}
					else
					{
						_numWorkerThreadsFree--;
					}
				}
			}

			if(!_numWorkerThreadsFree || !_workUnitQueue.count() )
			{
				// No worker threads are free or no work units need to be executed.

				// Wait on the next unit of work to be queued. This will unlock the mutex.
				_queueCond.wait();

				// Conditions mutex will be locked on exit of wait. Leave it locked for entry back into the start of the loop.
			}
		}

		_poolThreadActive = false;

		// Shutdown may have waited on pool thread becoming inactive.
		_queueCond.broadcast();

		// Release anything waiting on thread loop exit.
		_queueCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Thread Pool exited unexpectedly:";
		msg += ex.getSubsystemErrorString() + "\n";
		LOG(Logger::LogLevel::ERROR, msg)
	}

	// May have reached here via exception. So make sure flag is set.
	_poolThreadActive = false;
}

void ThreadPool::workThreadFree()
{
	if(!_shutdown)
	{
		_queueCond.lockMutex();

		// Just to be safe, protect this write because any thread could modify it.
		// Not strictly necessary at the moment though. Might change later.
		_numWorkerThreadsFree++;

		_queueCond.signal();
		_queueCond.unlockMutex();
	}
}

bool ThreadPool::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	bool canExecute = false;

	_queueCond.lockMutex();

	if(_poolThreadActive && !_shutdown && !getQuit())
	{
		// Pointer list does not own work unit. This is just a queue.
		// Rely on the worker thread deleting it.
		_workUnitQueue.append(workUnit, false);
		_queueCond.signal();

		canExecute = true;
	}

	_queueCond.unlockMutex();

	return canExecute;
}

unsigned ThreadPool::getViableWorkerThreadCount()
{
	return _viableWorkerThreadCount;
}

void ThreadPool::shutdown()
{
	// NOTE: Regardless of whether the pool thread is running shutdown still needs to be completed.

	// Flag the shutdown.
	_queueCond.lockMutex();
	_shutdown = true;

	// Wake up anything waiting on queue condition.
	_queueCond.broadcast();
	_queueCond.unlockMutex();

	// Need to obtain lock again to check thread active state.
	_queueCond.lockMutex();

	if(_poolThreadActive)
	{
		// Thread loop hasn't finished yet. Wait for it to signal.
		// This will release mutex.
		_queueCond.wait();
	}

	// Mutex is now locked.

	// Any uninvoked work units should be aborted.
	ThreadPoolWorkUnit* workUnit = _workUnitQueue.first();

	while(workUnit)
	{
		_workUnitQueue.remove();
		_queueCond.unlockMutex();

		workUnit -> abort();

		_queueCond.lockMutex();
		workUnit = _workUnitQueue.first();
	}

	_queueCond.unlockMutex();

	// Try and gracefully shutdown the worker threads.
	// Assume at this stage that _shutdown being set guards the array.
	for(unsigned index = 0; index < _numThreads; index++)
	{
		try
		{
			if(_pool[index]) _pool[index] -> shutDown();
		}
		catch(ThreadException& ex)
		{
			std::cout << "There was an issue while shutting down a thread pool worker thread." <<
				ex.getSubsystemErrorString() << "\n";
		}
	}

	// Finally unblock anything still waiting on mutex.
	_queueCond.unlockMutex();
}

void ThreadPool::enumerateState(unsigned numTabs)
{
	string indentTabs = "";
	for(unsigned level = 0; level < numTabs; level++)
	{
		indentTabs += "\t";
	}

	cout << indentTabs << "thread pool debug marker: " << threadPool -> _debugMarker << "\n";

	cout << indentTabs << "thread pool worker threads:\n";

	for(unsigned index = 0; index < _numThreads; index++)
	{
		cout << indentTabs << "\tthread index:" << index << "\n";
		if(_pool[index]) _pool[index] -> enumerateState(numTabs + 2);
	}
}