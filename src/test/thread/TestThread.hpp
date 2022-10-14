#include "../../thread/thread.hpp"
#include "../../thread/ThreadException.hpp"

#ifndef TEST_THREAD_H
#define TEST_THREAD_H

#include <string>

/**
 * Create and invoke thread entry point.
 */
class TestThread : public Thread
{
    public:

		// Really shit code but because it is very tightly coupled to the test I really couldn't be
		// bothered writing getters and setters.

		bool threadStarted;

		bool condWaiting;

		// True if there was an error during condition wait.
		bool condWaitError;

		// Keep a copy of the last execption caused condition based stuff.
		ThreadException* lastCondExcept;

		virtual ~TestThread()
		{
			if(lastCondExcept != nullptr) delete lastCondExcept;
		}

        TestThread()
		{
			threadStarted = false;
			_condToWaitOn = nullptr;
			_condToWaitOnTimeout = 0;
			condWaiting = false;
			condWaitError = false;
			lastCondExcept = nullptr;
		}

        virtual void threadEntry()
		{
            // Unit testing stuff ...

            threadStarted = true;

            while(!getQuit())
			{
				// To stop thrashing, sleep for a small time.
				nanoSleep(0, 1000);

				// Check for condition to wait on.
				if(_condToWaitOn != nullptr)
				{
					condWaitError = false;

					try
					{
						condWaiting = true;

						// Wait or wait timeout on the condition.
						if(_condToWaitOnTimeout > 0)
						{
							_condToWaitOn -> waitTimeout(_condToWaitOnTimeout);
						}
						else
						{
							_condToWaitOn -> wait();
						}

						condWaiting = false;

						_condToWaitOn -> unlockMutex();

						// To avoid infinite number of waits. Only wait on condition once.
						_condToWaitOn = nullptr;

						if(_condToSignal != nullptr)
						{
							_condToSignal -> lockMutex();
							_condToSignal -> broadcast();
							_condToSignal -> unlockMutex();
						}
					}
					catch(ThreadException& except)
					{
						setLastCondExcept(except);
						_condToWaitOn = nullptr;
					}
				}
			}
        }

		void waitOnCond(ThreadCondition* cond, ThreadCondition* signalCond)
		{
			// Immediately lock the conditions mutex which gives the calling test a mechanism to get the signal timing correct.
			cond -> lockMutex();

			condWaitError = false;
			_condToSignal = signalCond;
			_condToWaitOnTimeout = 0;
			_condToWaitOn = cond;
		}

		void waitOnCond(ThreadCondition* cond, unsigned int timeout, ThreadCondition* signalCond)
		{
			// Immediately lock the conditions mutex which gives the calling test a mechanism to get the signal timing correct.
			cond -> lockMutex();

			condWaitError = false;
			_condToSignal = signalCond;
			_condToWaitOnTimeout = timeout;
			_condToWaitOn = cond;
		}

    private:

		// Thread condition for this thread to wait on next time it wakes up.
		ThreadCondition* _condToWaitOn;
		unsigned int _condToWaitOnTimeout; // In milliseconds.
		// Once wait on condition to complete. signal this condition.
		ThreadCondition* _condToSignal;

		void setLastCondExcept(ThreadException except)
		{
			if(lastCondExcept != nullptr) delete lastCondExcept;
			lastCondExcept = new ThreadException(except);
		}
};

#endif