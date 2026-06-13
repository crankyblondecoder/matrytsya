#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <queue>

class ThreadPoolWorkThread;

#include "Thread.hpp"
#include "ThreadCondition.hpp"
#include "ThreadPoolWorkUnit.hpp"

/**
 * Pool of threads that can be allocated units of work to complete.
 */
class ThreadPool : private Thread
{
    public:

        virtual ~ThreadPool();

		/**
		 * @param numThreads Number of threads to initially allocate within pool.
		 * @throws ThreadException.
		 */
        ThreadPool(unsigned numThreads);

		/**
		 * Call point so that pool worker thread can notify this pool that it is now free to execute
		 * a work unit. Does not need to give specific worker thread because all work units are allocated
		 * to all available threads in a block.
		 */
		void workerThreadFree();

		/**
		 * Execute a work unit using one of the threads from the pool.
		 * @note The work unit will be deleted by this pool once it has completed or failure to execute.
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

		/**
		 * Wait on the the thread pool becoming active.
		 * @returns True if became active before a reasonable timeout period.
		 */
		bool waitOnBecomingActive();

		/** For debug. */
		bool _debugMarker;

		/** Enumerate the state of this thread pool. */
		void enumerateState(unsigned numTabs);

    protected:

		// This was public in the base class.
		void threadEntry() override;

		void _quitRequested() override;

    private:

		/// Number of worker threads in pool.
		unsigned _numThreads;

        /// Pool of worker threads.
		ThreadPoolWorkThread** _pool;

		/// Guards the worker thread pool.
		ThreadCondition _workerThreadPoolCond;

		/// Essentially the number of worker threads that were started successfully.
		unsigned _viableWorkerThreadCount;

		/// The number of worker threads that are available to do work.
		std::atomic<unsigned> _numWorkerThreadsFree;

		/// Queue for work units that need to be executed.
		std::queue<ThreadPoolWorkUnit*> _workUnitQueue;

		/**
		 * Condition for work unit processing thread to wait on.
		 * This guards the work unit queue.
		 */
		ThreadCondition _workUnitQueueCond;

		/// Index of last thread that was allocated a work unit. Allows for round robin style of work unit allocation.
		unsigned _lastAllocThreadIndex;

		/// Flag to indicate that pool thread is active.
		bool _poolThreadActive;

		/// Guards the pool thread active flag.
		ThreadCondition _poolThreadActiveCondition;

		/// Shutdown flag. True if shutdown or in the process of shutting down.
		std::atomic<bool> _shutdown;

		/**
		 * Gracefully shutdown this thread pool.
		 * This will try and nicely stop the pool thread and all worker threads.
		 */
		void __shutdown();
};

#endif
