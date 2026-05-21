#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "ThreadBase.hpp"
#include "ThreadException.hpp"

// -- Non class thread entry --
void* _threadEntry(void* thread)
{
	((ThreadBase*)thread) -> __threadEntry();

    return NULL;
}

ThreadBase::~ThreadBase()
{
}

ThreadBase::ThreadBase()
{
}

void ThreadBase::start()
{
    // Do not start a new thread if one is already running.
    if(!_threadRunning)
    {
		if(_start(&_threadEntry)) _threadRunning = true;
    }
    else
    {
		// Should never have been started twice.
		throw ThreadException(ThreadException::THREAD_ALREADY_STARTED);
    }
}

void ThreadBase::__threadEntry()
{
    // Invoke derived class thread entry.
    threadEntry();

	// Use stop conditions mutex to guard against a race condition where signalling/forcing stop waits on the condition after
	// it has already been broadcast.
	_stopCond.lockMutex();

    _threadRunning = false;

    // Wake up anything waiting on the thread to quit. Because the mutex those threads have to contend for is already locked
	// by this thread they won't exit condition wait until they can aquire the mutex.
    _stopCond.broadcast();

	// This should allow the threads blocked on the stop condition to continue processing.
	_stopCond.unlockMutex();
}

bool ThreadBase::signalStop()
{
	// Make sure the thread can't exit before we are ready waiting for it to do so.
	// This mutex _must_ be obtained around the test for thread running.
	_stopCond.lockMutex();

    if(_threadRunning)
    {
        // Block until thread exits or timeout occurs.
        _quit = true;
        // Make sure stop condition doesn't cause permanent block.
        _stopCond.waitTimeout(2000);
    }

	// This must occur for stop condition to be in consistent state.
	_stopCond.unlockMutex();

    return !_threadRunning;
}

void ThreadBase::forceStop()
{
	// Make sure the thread can't exit before we are ready waiting for it to do so.
	// This mutex _must_ be obtained around the test for thread running.
	_stopCond.lockMutex();

    if(_threadRunning)
    {
		try
		{
			_forceStop();
		}
		catch(ThreadException &ex)
		{
			_stopCond.unlockMutex();

			throw;
		}

		_stopCond.waitTimeout(2000);
    }

	_stopCond.unlockMutex();
}

bool ThreadBase::getRunning()
{
    return _threadRunning;
}

unsigned int ThreadBase::sleep(unsigned int seconds)
{
	// Standard unix call.
	return ::sleep(seconds);
}

void ThreadBase::nanoSleep(int seconds, long nanoSeconds)
{
	timespec sleepDuration;
	sleepDuration.tv_sec = seconds;
	sleepDuration.tv_nsec = nanoSeconds;

	int error = nanosleep(&sleepDuration, NULL);

	if(error)
	{
		switch(error)
		{
			case EFAULT:

				throw ThreadException(ThreadException::THREAD_GENERAL_ERROR, error,
					"EFAULT Problem with copying information from user space.");
				break;

			case EINTR:

				throw ThreadException(ThreadException::THREAD_SLEEP_INTERRUPTED, error,
					"EINTR The pause has been interrupted by a signal that was delivered to the thread.");
				break;

			case EINVAL:

				throw ThreadException(ThreadException::THREAD_INCORRECT_PARAMETER, error,
					"EINVAL The value in the tv_nsec field was not in the range 0 to 999999999 or tv_sec was negative.");
				break;
		}
	}
}

bool ThreadBase::getQuit()
{
    return _quit;
}
