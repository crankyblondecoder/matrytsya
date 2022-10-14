#ifndef THREAD_POOL_H
#define THREAD_POOL_H

class ThreadPoolWorkThread;

#include "Thread.hpp"
#include "ThreadCondition.hpp"
#include "ThreadMutex.hpp"
#include "ThreadPoolWorkUnit.hpp"
#include "../util/ptrList.hpp"

/**
 * Pool of threads that can be allocated units of work to complete.
 */
class ThreadPool : private Thread
{
    public:

        virtual ~ThreadPool();

		/**
		 * @param numThreads Number of threads to initially allocate within pool.
		 */
        ThreadPool(unsigned numThreads);

		/**
		 * Entry point so that pool work thread can notify this pool that it is now free to execute
		 * a work unit. Does not need to give specific work thread because all work units are allocated
		 * to all available threads in a block.
		 */
		void workThreadFree();

		/**
		 * Execute a work unit using one of the threads from the pool.
		 * @note The work unit will be deleted by this pool once it has completed.
		 * @param workUnit Work unit to execute.
		 * @returns True if could execute. False otherwise.
		 */
		bool executeWorkUnit(ThreadPoolWorkUnit* workUnit);

		/**
		 * Get the current number of viable worker threads this pool has.
		 * Some threads may not have been able to start. So the pool may still be viable, but with a reduced capacity.
		 */
		unsigned getViableWorkerThreadCount();

		/**
		 * Gracefully shutdown this thread pool.
		 * This will try and nicely stop the pool thread and all worker threads.
		 */
		void shutdown();

    protected:

		void threadEntry();

    private:

		/// Number of worker threads in pool.
		unsigned _numThreads;

        // Pool of worker threads.
		ThreadPoolWorkThread** _pool;

		/// Essentially the number of worker threads that were started successfully.
		unsigned _viableWorkerThreadCount;

		/// The number of worker threads that are available to do work.
		unsigned _numWorkerThreadsFree;

		/// Condition for work unit processing thread to wait on.
		ThreadCondition _queueCond;

		/// Queue for work units that need to be executed.
		ptrList<ThreadPoolWorkUnit> _workUnitQueue;

		/// Index of last thread that was allocated a work unit. Allows for round robin style of work unit allocation.
		unsigned _lastAllocThreadIndex;

		/// If true already allocating work units to workers.
		bool _workUnitAllocPassInProgress;

		/// Flag to indicate that pool thread is active.
		bool _poolThreadActive;

		/// Shutdown flag. True if shutdown or in the process of shutting down.
		bool _shutdown;

		void workUnitAllocPass();
};

#endif
