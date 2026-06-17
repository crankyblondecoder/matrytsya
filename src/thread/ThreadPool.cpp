#include "ThreadPool.hpp"

#include <iostream>
#include <new>
#include <stdexcept>
#include "ThreadException.hpp"
#include "ThreadPoolWorkThread.hpp"

using namespace std;

// TODO How can threads be pre-empted if they run for too long?
//      Maybe there is a way to downgrade a threads priority while it is running?
//      Maybe there is a way to kill a thread from outside the thread?
//      Needs to be some way of notifying work units so that any associated resources can be released.

ThreadPool::~ThreadPool()
{
	unsigned index;

	// The pool thread must be stopped first.
	// Ideally this should have been done gracefully way before destruction.
	stop(true);

	// Force the worker threads to stop.
	// Again, ideally that should have already been done gracefully.
	// Delete once stopped.
	for(index = 0; index < _numThreads; index++)
	{
		if(_pool[index])
		{
			_pool[index] -> stop(true);
			delete _pool[index];
		}
	}

	// Deallocate everything else.

	delete[] _pool;
}

ThreadPool::ThreadPool(unsigned numThreads)
{
	_debugMarker = false;
	_viableWorkerThreadCount = 0;
	_numWorkerThreadsFree = 0;
	_numThreads = numThreads;
	_lastAllocThreadIndex = 0;
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
		bool running = false;

		try
		{
			_pool[index] -> start();

			running = _pool[index] -> waitForReady(2000);

			if(running) _viableWorkerThreadCount++;
		}
		catch(ThreadException& ex)
		{
			running = false;
		}

		if(!running)
		{
			_pool[index] -> shutDown();
			delete _pool[index];
			_pool[index] = 0;

			cout << "Thread with index: " << index << " could not be started and has been terminated.\n";
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

bool ThreadPool::waitOnBecomingActive()
{
	_poolThreadActiveCondition.lockMutex();

	unsigned loopLimit = 5;

	try
	{
		while(!_poolThreadActive && !_shutdown && loopLimit--) _poolThreadActiveCondition.waitTimeout(500);
	}
	catch(ThreadException& ex)
	{
		_poolThreadActiveCondition.unlockMutex();
		throw;
	}

	bool active = _poolThreadActive;

	_poolThreadActiveCondition.unlockMutex();

	return active;
}

void ThreadPool::threadEntry()
{
	unsigned nextAllocThreadIndex;
	bool allocated;
	unsigned numAllocTrys;
	ThreadPoolWorkThread* workerThread;

	// Note: The policy is that any kind of major error in the thread pool queue procesing should be treated as a
	//       critical error and immediately shut down queue processing. If the error is from the mutex or condition
	//       related guards, it should cause the program to abort entirely.

	try
	{
		_poolThreadActiveCondition.lockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: First thread active condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	_poolThreadActive = true;

	try
	{
		_poolThreadActiveCondition.broadcast();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: First thread active condition broadcast failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_poolThreadActiveCondition.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: First thread active condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	// Queue condition mutex is locked.

	while(!_shutdown)
	{
		// Assume number of worker threads free can _only_ be decremented by this loop.
		// Therefore, this does not require guarding.

		if(_numWorkerThreadsFree)
		{
			// Get a work unit from the queue.
			ThreadPoolWorkUnit* workUnit = 0;

			try
			{
				_workUnitQueueCond.lockMutex();
			}
			catch(ThreadException& ex)
			{
				std::string msg = "Critical error: Queue condition mutex lock failed -> " +
					ex.getSubsystemErrorString();

				// This is so severe that the app should just be killed.
				throw std::runtime_error(msg);
			}

			if(!_workUnitQueue.empty())
			{
				// Once removed from queue, work unit _must_ be ultimately deleted in this function.
				workUnit = _workUnitQueue.front();
				_workUnitQueue.pop();
			}

			try
			{
				_workUnitQueueCond.unlockMutex();
			}
			catch(ThreadException& ex)
			{
				if(workUnit)
				{
					workUnit -> abort();
					delete workUnit;
				}

				std::string msg = "Critical error: Queue condition mutex unlock failed -> " +
					ex.getSubsystemErrorString();

				// This is so severe that the app should just be killed.
				throw std::runtime_error(msg);
			}

			if(workUnit)
			{
				// Allocate work unit to thread using simple round robin algorithm.
				// Only this thread controls the thread pool allocation so no need to guard it.

				numAllocTrys = 0;
				allocated = false;
				nextAllocThreadIndex = _lastAllocThreadIndex + 1;

				while(!_shutdown && !allocated && numAllocTrys < _numThreads)
				{
					if(nextAllocThreadIndex >= _numThreads) nextAllocThreadIndex = 0;

					workerThread = _pool[nextAllocThreadIndex];

					if(!_shutdown && workerThread)
					{
						// Rely on the worker thread knowing if it can execute the work unit or not. This way we
						// don't need to track allocated worker thread.
						// Assume this call won't block.
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

				if(!allocated)
				{
					if(!_shutdown)
					{
						workUnit -> abort();
						delete workUnit;

						std::string msg = "Critical error: ThreadPool was not able to allocate work unit to worker thread. This was unexpected.\n";

						// This is so severe that the app should just be killed.
						throw std::runtime_error(msg);
					}
					else
					{
						workUnit -> abort();
						delete workUnit;
					}
				}
				else
				{
					_numWorkerThreadsFree--;
				}
			}
		}

		// Short circuit on shutdown.
		if(_shutdown) break;

		// Wait for worker thread to become available if none are already.

		try
		{
			_workerThreadPoolCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Worker thread pool condition mutex lock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		// To avoid deadlock, This relies on shutdown trying to obtain the lock on the queue condition.
		if(!_numWorkerThreadsFree && !_shutdown)
		{
			try
			{
				// Wait on worker threads becoming free or shutdown being invoked.
				_workerThreadPoolCond.wait();
			}
			catch(ThreadException& ex)
			{
				std::string msg = "Critical error: Worker thread pool condition wait failed -> " +
					ex.getSubsystemErrorString();

				// This is so severe that the app should just be killed.
				throw std::runtime_error(msg);
			}
		}

		try
		{
			_workerThreadPoolCond.unlockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Worker thread pool condition mutex unlock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		// Short circuit on shutdown.
		if(_shutdown) break;

		// Wait for a work unit to be posted if none are ready in the queue.

		try
		{
			_workUnitQueueCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Queue condition mutex lock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		// To avoid deadlock, This relies on shutdown trying to obtain the lock on the queue condition.
		if(_workUnitQueue.empty() && !_shutdown)
		{
			// No work units need to be executed so wait for one.

			try
			{
				// Wait on the next unit of work to be queued or shutdown to be invoked.
				_workUnitQueueCond.wait();
			}
			catch(ThreadException& ex)
			{
				std::string msg = "Critical error: Queue condition wait failed -> " +
					ex.getSubsystemErrorString();

				// This is so severe that the app should just be killed.
				throw std::runtime_error(msg);
			}
		}

		try
		{
			_workUnitQueueCond.unlockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Queue condition mutex unlock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}
	}

	// Set the thread pool as being inactive.

	try
	{
		_poolThreadActiveCondition.lockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Final thread active condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	_poolThreadActive = false;

	try
	{
		_poolThreadActiveCondition.broadcast();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Final thread active condition broadcast failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_poolThreadActiveCondition.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Final thread active condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}
}

void ThreadPool::workerThreadFree()
{
	if(!_shutdown)
	{
		try
		{
			_workerThreadPoolCond.lockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Worker thread pool condition mutex lock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		_numWorkerThreadsFree++;

		try
		{
			_workerThreadPoolCond.broadcast();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Worker thread pool condition broadcast failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		try
		{
			_workerThreadPoolCond.unlockMutex();
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Worker thread pool condition mutex unlock failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}
	}
}

bool ThreadPool::executeWorkUnit(ThreadPoolWorkUnit* workUnit)
{
	bool canExecute = false;

	try
	{
		_workUnitQueueCond.lockMutex();
	}
	catch(ThreadException& ex)
	{
		workUnit -> abort();
		delete workUnit;

		std::string msg = "Critical error: Queue condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	if(!_shutdown)
	{
		// Queue does not own work unit pointer.
		// Rely on the worker thread deleting it.

		_workUnitQueue.push(workUnit);

		try
		{
			_workUnitQueueCond.broadcast();
		}
		catch(ThreadException& ex)
		{
			_workUnitQueueCond.unlockMutex();

			std::string msg = "Critical error: Queue condition broadcast failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}

		canExecute = true;
	}
	else
	{
		workUnit -> abort();
		delete workUnit;

		std::cout << "Warning: Work unit could not be executed. You probably weren't expecting this.\n";
	}

	try
	{
		_workUnitQueueCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Queue condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	return canExecute;
}

unsigned ThreadPool::getViableWorkerThreadCount()
{
	return _viableWorkerThreadCount;
}

void ThreadPool::_quitRequested()
{
	// This is triggered by the underlying thread that is used for managing the pool.
	__shutdown();
}

void ThreadPool::shutdown()
{
	__shutdown();
}

void ThreadPool::__shutdown()
{
	// NOTE: Regardless of whether the pool thread is running shutdown still needs to be completed.
	//       It tries to do this gracefully, but ultimately forcefully.

	_shutdown = true;

	// Unblock any waits on the worker thread pool or the work unit queue so the pool thread can finish and become
	// inactive.

	try
	{
		_workerThreadPoolCond.lockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Worker thread pool condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_workerThreadPoolCond.broadcast();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Worker thread pool condition broadcast failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_workerThreadPoolCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Worker thread pool condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_workUnitQueueCond.lockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Queue condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_workUnitQueueCond.broadcast();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Queue condition broadcast failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	try
	{
		_workUnitQueueCond.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Queue condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	// Wait for pool thread to become inactive.

	try
	{
		_poolThreadActiveCondition.lockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Pool thread active condition mutex lock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	if(_poolThreadActive)
	{
		try
		{
			// Thread loop hasn't finished yet. Impatiently wait for it to end.
			unsigned loopLimit = 5;

			while(_poolThreadActive && loopLimit--)
			{
				_poolThreadActiveCondition.waitTimeout(500);
			}
		}
		catch(ThreadException& ex)
		{
			std::string msg = "Critical error: Pool thread active condition wait failed -> " +
				ex.getSubsystemErrorString();

			// This is so severe that the app should just be killed.
			throw std::runtime_error(msg);
		}
	}

	try
	{
		_poolThreadActiveCondition.unlockMutex();
	}
	catch(ThreadException& ex)
	{
		std::string msg = "Critical error: Pool thread active condition mutex unlock failed -> " +
			ex.getSubsystemErrorString();

		// This is so severe that the app should just be killed.
		throw std::runtime_error(msg);
	}

	// Any uninvoked work units should be aborted. Assume that the queue no longer needs to be guarded because no
	// other pathway can now modify it.

	if(!_workUnitQueue.empty())
	{
		ThreadPoolWorkUnit* workUnit = _workUnitQueue.front();

		while(workUnit)
		{
			_workUnitQueue.pop();

			workUnit -> abort();

			delete workUnit;

			if(!_workUnitQueue.empty())
			{
				workUnit = _workUnitQueue.front();
			}
			else
			{
				workUnit = 0;
			}
		}
	}

	// Try and gracefully shutdown the worker threads.
	// Assume that no other pathway can now modify the array.

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
}

void ThreadPool::enumerateState(unsigned numTabs)
{
	string indentTabs = "";
	for(unsigned level = 0; level < numTabs; level++)
	{
		indentTabs += "\t";
	}

	cout << indentTabs << "thread pool worker threads:\n";

	for(unsigned index = 0; index < _numThreads; index++)
	{
		cout << indentTabs << "\tthread index:" << index << "\n";
		if(_pool[index]) _pool[index] -> enumerateState(numTabs + 2);
	}
}
