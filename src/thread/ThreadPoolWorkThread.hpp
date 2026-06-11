#ifndef THREAD_POOL_WORK_THREAD_H
#define THREAD_POOL_WORK_THREAD_H

#include <atomic>

using namespace std;

class ThreadPool;

#include "ThreadCondition.hpp"
#include "ThreadPool.hpp"
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
		 * Wait for the thread to be ready to process work units.
		 * @param timeout Maximumn time in ms to wait.
		 * @returns True if ready to process work units. False if not.
		 */
		bool waitForReady(unsigned timeout);

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

		/** Enumerate the state of this thread pool. */
		void enumerateState(unsigned numTabs);

    protected:

		// Public in base class.
        void threadEntry() override;

		void _quitRequested() override;

    private:

		/**
		 * Whether the work unit can be processed by this work thread.
		 */
		bool __canAcceptWorkUnit();

		/// Thread pool this worker belongs to.
		ThreadPool* _threadPool;

		/// The current unit of work that is being executed by the thread.
		ThreadPoolWorkUnit* _curWorkUnit;

		/// Condition that guards the current work unit.
		ThreadCondition _curWorkUnitCond;

		/// Flag to indicate that worker thread is active.
		std::atomic<bool> _workerThreadActive;

 		/// Condition that guards the worker thread active flag.
		ThreadCondition _workerThreadActiveCond;

		/// Flag to indicate a work unit is being executed.
		std::atomic<bool> _working;

		/// Flag to indicate shutdown is requested.
		std::atomic<bool> _shutdown;

		bool _debugMarker;
};

#endif
