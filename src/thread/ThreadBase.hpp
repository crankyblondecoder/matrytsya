#ifndef THREAD_BASE_H
#define THREAD_BASE_H

#include "ThreadCondition.hpp"

/**
 * Base class for create and invoke thread entry point.
 */
class ThreadBase
{
    public:

        virtual ~ThreadBase();
        ThreadBase();

		/**
		 * Start the thread.
		 * @throw ThreadException
		 */
		virtual void start() final;


        /**
         * Signal (request) the thread to stop.
		 * This is a "gentle" request in that it just sets the threads quit flag and waits for it to react.
         * Waits for thread to stop for up to 2 seconds.
         * @returns Whether the thread was successfully stopped.
         */
        virtual bool signalStop();

        /**
         * Forcibly stop this thread.
		 * This requires the thread to stop and will wait for it to do so for up to 2 seconds before returning.
		 * @throw ThreadException
         */
        virtual void forceStop() final;

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
		 * Subclass hook so that concrete thread implementation can start the thread.
		 * @throw ThreadException
		 */
		virtual void _forceStop() = 0;

		/**
		 * Get the quit flag of this thread.
		 * If set to true the current code running on the thread should gracefully exit.
		 */
        bool getQuit();

    private:

        bool _quit;

        // It does not make sense to copy a thread so do not allow it.
        ThreadBase(const ThreadBase& copyFrom);
        ThreadBase& operator= (const ThreadBase& copyFrom);

        bool _threadRunning = false;

        /** Used to wait for thread to quit. */
        ThreadCondition _stopCond;
};

#endif
