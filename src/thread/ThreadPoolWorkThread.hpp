#ifndef THREAD_POOL_WORK_THREAD_H
#define THREAD_POOL_WORK_THREAD_H

class ThreadPool;

#include "../thread/thread.hpp"
#include "ThreadPoolWorkUnit.hpp"

/**
 * Thread that is a worker thread in a thread pool.
 */
class ThreadPoolWorkThread : public Thread
{
    public:

        virtual ~ThreadPoolWorkThread();

        ThreadPoolWorkThread(ThreadPool* threadPool);

		/**
		 * Gracefully shutdown this thread.
		 */
		void shutDown();

		/**
		 * Execute a single unit of work.
		 * The work unit is owned by this and will be delete by it once executed.
		 * @param workUnit Work unit to execute.
		 * @returns True if work unit accepted. False otherwise.
		 */
		bool executeWorkUnit(ThreadPoolWorkUnit* workUnit);

		/**
		 * Determine if this can currently accept a work unit.
		 * @returns True if can accept a work unit. False otherwise.
		 */
		bool canAcceptWorkUnit();

    protected:

        /** Entry point when thread is started */
        void threadEntry();

    private:

		/** Thread pool this worker belongs to. */
		ThreadPool* _threadPool;

		/** Condition so that this worker can block until a work unit is available for it. */
		ThreadCondition _cond;

		/** The current unit of work that is being executed by the thread. */
		ThreadPoolWorkUnit* _curWorkUnit;

		/** Flag to indicate that worker thread is active. */
		bool _workerThreadActive;

		/** Flag to indicate a work unit is being executed. */
		bool _working;

		/** Flag to indicate shutdown is requested. */
		bool _shutdown;

		bool _canAcceptWorkUnit();
};

#endif
