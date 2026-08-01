#include "../../thread/Thread.hpp"
#include "../../thread/ThreadException.hpp"

#ifndef TEST_THREAD_H
#define TEST_THREAD_H

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

		// What ThreadBase::currentThreadStopping() answered while the thread was still looping, and what it
		// answered once the loop had been asked to quit.
		bool stoppingWhileRunning;
		bool stoppingAfterQuit;

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
			stoppingWhileRunning = false;
			stoppingAfterQuit = false;
		}

        virtual void threadEntry()
		{
            // Unit testing stuff ...

            threadStarted = true;

            while(!_getQuit())
			{
				// To stop thrashing, sleep for a small time.
				nanoSleep(0, 1000);

				// Nothing has asked this thread to stop yet, so anything it is carrying must be told it can
				// carry on.
				if(ThreadBase::currentThreadStopping()) stoppingWhileRunning = true;

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

			// Read before the entry point returns, as that is where the thread stops publishing itself.
			stoppingAfterQuit = ThreadBase::currentThreadStopping();
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

	protected:

		void _quitRequested() override {}

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
