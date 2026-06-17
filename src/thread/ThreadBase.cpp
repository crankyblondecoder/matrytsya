#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "../log/log.hpp"
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
	// The destructor can't stop the running thread because that is done by an implementing sub-class which has
	// already been destructed.
}

ThreadBase::ThreadBase()
{
	_quit = false;
	_started = false;
	_stopped = false;
}

bool ThreadBase::start()
{
	// Dear Claude, yes a thread can only be started (used) once, i.e. It is "one shot".

	_flagMutex.lock();

    // Can only be started once.

	if(_started)
	{
		_flagMutex.unlock();

		// Should never have been started twice.
		throw ThreadException(ThreadException::THREAD_ALREADY_STARTED);
	}

	_started = true;

	_flagMutex.unlock();

    bool startSuccess = _start(&_threadEntry);

	if(!startSuccess)
	{
		_stopCond.lockMutex();
		_stopped = true;
		_stopCond.unlockMutex();
	}

	return startSuccess;
}

void ThreadBase::__threadEntry()
{
    // Invoke derived class thread entry.
    threadEntry();

	_stopCond.lockMutex();

	_stopped = true;

	// Wake up anything waiting on the thread to stop.
    _stopCond.broadcast();

	_stopCond.unlockMutex();
}

bool ThreadBase::stop(bool force)
{
	_flagMutex.lock();

	if(!_started)
	{
		LOG(Logger::WARN, "Warning: You are trying to stop a thread that hasn't started.\n")

		_flagMutex.unlock();
		return false;
	}

	_flagMutex.unlock();

	_stopCond.lockMutex();

	if(_stopped)
	{
		_stopCond.unlockMutex();
		return true;
	}

	_stopCond.unlockMutex();

	_quit = true;

	_quitRequested();

	// Now wait for the thread to terminate.

	_stopCond.lockMutex();

   	unsigned loopLimit = 5;

	try
	{
	    while(!_stopped && loopLimit--)
		{
			_stopCond.waitTimeout(500);
	    }
	}
	catch(ThreadException& ex)
	{
		_stopCond.unlockMutex();
		throw;
	}

	bool hasStopped = _stopped;

	_stopCond.unlockMutex();

	if(!hasStopped && force)
	{
		_forceStop();

		_stopCond.lockMutex();

		loopLimit = 5;
		try
		{
			while(!_stopped && loopLimit--)
			{
				_stopCond.waitTimeout(500);
			}
		}
		catch(ThreadException& ex)
		{
			_stopCond.unlockMutex();
			throw;
		}

		_stopCond.unlockMutex();
	}

    return _stopped;
}

bool ThreadBase::getRunning()
{
	// Because the thread running flag is atomic, it doesn't need the flag mutex to guard it.
	return _started &&!_stopped;
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
		// errno holds the error number, _not_ error.
		switch(errno)
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

bool ThreadBase::_getQuit()
{
	// Because the quit flag is atomic, it doesn't need the flag mutex to guard it.
    return _quit;
}
