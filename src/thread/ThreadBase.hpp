#ifndef THREAD_BASE_H
#define THREAD_BASE_H

#include <atomic>

#include "ThreadCondition.hpp"
#include "ThreadMutex.hpp"

/**
 * Base class for all thread types.
 * @note This can only be used (started) once, i.e. It is a "one shot".
 */
class ThreadBase
{
    public:

        virtual ~ThreadBase();
        ThreadBase();

		/**
		 * Start the thread.
		 * @note Can only be started once.
		 * @throw ThreadException
		 */
		virtual bool start() final;

        /**
         * Stop the thread.
		 * @param force If true, issue a force stop if gracefully stopping doesn't work.
         * @returns Whether the thread was successfully stopped.
         */
        virtual bool stop(bool force);

		/**
		 * Sleep for a specified number of whole seconds.
		 * @param seconds Number of seconds to sleep for.
		 * @returns unsigned int The number of seconds left to sleep if call was interrupted.
		 */
		unsigned int sleep(unsigned int seconds);

		/**
		 * High resolution sleep.
		 * @param seconds Number of seconds to sleep.
		 * @param nanoSeconds Number of nanoseconds to sleep.
		 * @throw ThreadException
		 */
		void nanoSleep(int seconds, long nanoSeconds);

		/**
		 * Get whether this thread is currently running.
		 */
		bool getRunning();

        /** Sub class thread entry point */
        virtual void threadEntry() = 0;

        /** Initial thread entry point. */
        void __threadEntry();

    protected:

		/**
		 * Subclass hook so that concrete thread implementation can start the thread.
		 * @param threadEntry Pure c function call that is used for thread entry once thread has started.
		 * @throw ThreadException
		 */
        virtual bool _start(void*(*threadEntry)(void*)) = 0;

		/**
		 * Sub-class hook so that concrete thread implementation can stop the thread.
		 * The sub-class should be able to handle the case where the thread has already stopped.
		 * @throw ThreadException
		 */
		virtual void _forceStop() = 0;

		/**
		 * Get the quit flag of this thread.
		 * If set to true the current code running on the thread should gracefully exit.
		 */
        bool _getQuit();

		/**
		 * Required sub-class hook to indicate this thread has requested to quit.
		 * The sub-class must be able to handle the case where the thread exits before this is called.
		 */
		virtual void _quitRequested() = 0;

    private:

        // It does not make sense to copy a thread so do not allow it.
        ThreadBase(const ThreadBase& copyFrom);
        ThreadBase& operator= (const ThreadBase& copyFrom);

		/// Flag to indicate that this thread should quit.
        std::atomic<bool> _quit;

		/// Flag to indicate that the thread has started.
		std::atomic<bool> _started;

		/// Flag to indicate that the thread has stopped.
		std::atomic<bool> _stopped;

		/// General mutex used to guard all flags.
		ThreadMutex _flagMutex;

        /// Used to wait for thread to stop.
        ThreadCondition _stopCond;
};

#endif
