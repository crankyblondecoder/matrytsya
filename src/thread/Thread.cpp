#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "Thread.hpp"
#include "ThreadException.hpp"

// -- Non class thread entry --
void* _threadEntry(void* thread)
{
    ((Thread*)thread) -> __threadEntry();

    return NULL;
}

Thread::~Thread()
{
}

Thread::Thread()
{
}

void Thread::start()
{
    // Do not start a new thread if one is already running.
    if(!_threadRunning)
    {
        int error = pthread_create(&_thread, NULL, &_threadEntry, (void*) this);

        switch(error)
        {
            case EAGAIN:

                throw ThreadException(ThreadException::THREAD_RESOURCES_EXHAUSTED, error,
                    "EAGAIN The system lacked the necessary resources to create another thread.");
                break;

            case EINVAL:

                throw ThreadException(ThreadException::THREAD_INCORRECT_PARAMETER, error,
                    "EINVAL The value specified by attr is invalid.");
                break;

            case EPERM:

                throw ThreadException(ThreadException::THREAD_INSUFFICIENT_PRIVILEGES, error,
                    "EPERM The caller does not have appropriate permission.");
                break;

            default:
                if(error) throw ThreadException(ThreadException::THREAD_GENERAL_ERROR, error);
                break;
        }

        _threadRunning = true;
    }
    else
    {
        // Should never have been started twice.
        throw ThreadException(ThreadException::THREAD_ALREADY_STARTED);
    }
}

void Thread::__threadEntry()
{
    // Invoke derived class thread entry.
    threadEntry();

	// Use stop conditions mutex to guard against a race condition where signalling/forcing stop waits on a the condition after
	// it has already been broadcast.
	_stopCond.lockMutex();

    _threadRunning = false;

    // Wake up anything waiting on the thread to quit. Because the mutex those threads have to contend for is already locked
	// by this thread they won't exit condition wait until they can aquire the mutex.
    _stopCond.broadcast();

	// This should allow the threads blocked on the stop condition to continue processing.
	_stopCond.unlockMutex();
}

bool Thread::signalStop()
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

void Thread::forceStop()
{
	// Make sure the thread can't exit before we are ready waiting for it to do so.
	// This mutex _must_ be obtained around the test for thread running.
	_stopCond.lockMutex();

    if(_threadRunning)
    {
        int error = pthread_cancel(_thread);

		if(error)
		{
			_stopCond.unlockMutex();

			switch(error)
			{
				case ESRCH:

					throw ThreadException(ThreadException::THREAD_INCORRECT_PARAMETER, error,
						"ESRCH Thread ID could not be found.");
					break;

				default:

					throw ThreadException(ThreadException::THREAD_GENERAL_ERROR, error);
					break;
			}
		}

		_stopCond.waitTimeout(2000);
    }

	_stopCond.unlockMutex();
}

bool Thread::getRunning()
{
    return _threadRunning;
}

unsigned int Thread::sleep(unsigned int seconds)
{
	// Standard unix call.
	return sleep(seconds);
}

void Thread::nanoSleep(int seconds, long nanoSeconds)
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

// -- Protected

bool Thread::getQuit()
{
    return _quit;
}