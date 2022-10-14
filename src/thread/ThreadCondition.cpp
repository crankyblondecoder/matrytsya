#include <errno.h>

#include "pthread_errors.hpp"
#include "ThreadCondition.hpp"
#include "ThreadException.hpp"

ThreadCondition::~ThreadCondition()
{
    // Do not handle any errors because at this point there is nothing that can be done about it.
    pthread_cond_destroy(&_condition);
}

ThreadCondition::ThreadCondition()
{
    int error = pthread_cond_init(&_condition, NULL);

	if(error)
	{
		switch(error)
		{
			case EAGAIN:

				throw ThreadException(ThreadException::CONDITION_RESOURCES_EXHAUSTED, error,
					"EAGAIN The system lacked the necessary resources (other than memory) to initialise another condition variable.");
				break;

			case ENOMEM:

				throw ThreadException(ThreadException::CONDITION_RESOURCES_EXHAUSTED, error,
					"ENOMEM Insufficient memory exists to initialize the condition variable.");
				break;

			case EBUSY:

				throw ThreadException(ThreadException::CONDITION_BUSY, error,
					"EBUSY Attempt to reinitialise not yet destroyed condition variable.");
				break;

			case EINVAL:

				throw ThreadException(ThreadException::CONDITION_INCORRECT_PARAMETER, error,
					"EINVAL The value specified by attr is invalid.");
				break;

			default:

				throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, error);
				break;
		}
	}

	// Initialise the mutex the condition requires to be waited upon.

	error = pthread_mutex_init(&_mutex, NULL);

	if(error)
	{
		// Attempt to destroy condition prior to throwing exception.
		pthread_cond_destroy(&_condition);

		std::string errorString = "Pthread mutex initialisation error while trying to construct condition. ";

		switch(error)
		{
			case EAGAIN:

				errorString += PTHREAD_MUTEX_INIT_EAGAIN;
				break;

			case ENOMEM:

				errorString += PTHREAD_MUTEX_INIT_ENOMEM;
				break;

			case EPERM:

				errorString += PTHREAD_MUTEX_INIT_EPERM;
				break;

			case EBUSY:

				errorString += PTHREAD_MUTEX_INIT_EBUSY;
				break;

			case EINVAL:

				errorString += PTHREAD_MUTEX_INIT_EINVAL;
				break;

			default:

				errorString += "Unknown pthread mutex error.";
				break;
		}

		throw ThreadException(ThreadException::CONDITION_MUTEX_ERROR, error, errorString.c_str());
	}
}

void ThreadCondition::wait()
{
    int error = pthread_cond_wait(&_condition, &_mutex);

	if(error)
	{
		switch(error)
		{
			case EINVAL:

				throw ThreadException(ThreadException::CONDITION_INCORRECT_PARAMETER, error,
					"EINVAL Invalid condition, mutex or abstime parameters or more than one mutex used for condition.");
				break;

			case EPERM:

				throw ThreadException(ThreadException::CONDITION_INSUFFICIENT_PRIVILEGES, error,
					"EPERM The mutex was not owned by the current thread at the time of the call.");
				break;

			default:

				throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, error);
				break;
		}
	}
}

bool ThreadCondition::waitTimeout(unsigned int timeOut)
{
    // Calculate absolute wait time as required by the pthread condition.
    struct timespec absTimeOutTime;
    int clockError = clock_gettime(CLOCK_REALTIME, &absTimeOutTime);
    if(clockError) throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, clockError, "clock_gettime error.");
    long fracSecMilli = (absTimeOutTime.tv_nsec / 1000000) + timeOut;
    absTimeOutTime.tv_sec += fracSecMilli / 1000;
    absTimeOutTime.tv_nsec = (fracSecMilli % 1000) * 1000000;

    int error = pthread_cond_timedwait(&_condition, &_mutex, &absTimeOutTime);

    if(error == ETIMEDOUT) return true;

	if(error)
	{
		switch(error)
		{
			case EINVAL:

				throw ThreadException(ThreadException::CONDITION_INCORRECT_PARAMETER, error,
					"EINVAL Invalid condition, mutex or abstime parameters or more than one mutex used for condition.");
				break;

			case EPERM:

				throw ThreadException(ThreadException::CONDITION_INSUFFICIENT_PRIVILEGES, error,
					"EPERM The mutex was not owned by the current thread at the time of the call.");
				break;

			default:

				throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, error);
				break;
		}
	}

    return false;
}

void ThreadCondition::signal()
{
    int error = pthread_cond_signal(&_condition);

	if(error)
	{
		switch(error)
		{
			case EINVAL:

				throw ThreadException(ThreadException::CONDITION_INCORRECT_PARAMETER, error,
					"EINVAL The condition variable does not refer to an initialized condition.");
				break;

			default:

				throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, error);
				break;
		}
	}
}

void ThreadCondition::broadcast()
{
   int error = pthread_cond_broadcast(&_condition);

	if(error)
	{
		switch(error)
		{
			case EINVAL:

				throw ThreadException(ThreadException::CONDITION_INCORRECT_PARAMETER, error,
					"EINVAL The condition variable does not refer to an initialized condition.");
				break;

			default:

				throw ThreadException(ThreadException::CONDITION_GENERAL_ERROR, error);
				break;
		}
	}
}

void ThreadCondition::appendPthreadMutexLockError(int errorCode, std::string& errorString)
{
	switch(errorCode)
	{
		case EINVAL:

			errorString += PTHREAD_MUTEX_LOCK_EINVAL;
			break;

		case EAGAIN:

			errorString += PTHREAD_MUTEX_LOCK_EAGAIN;
			break;

		case EDEADLK:

			errorString += PTHREAD_MUTEX_LOCK_EDEADLK;
			break;
	}
}

void ThreadCondition::appendPthreadMutexUnlockError(int errorCode, std::string& errorString)
{
	 switch(errorCode)
    {
        case EINVAL:

            errorString += PTHREAD_MUTEX_UNLOCK_EINVAL;
            break;

        case EAGAIN:

            errorString += PTHREAD_MUTEX_UNLOCK_EAGAIN;
            break;

        case EPERM:

            errorString += PTHREAD_MUTEX_UNLOCK_EPERM;
            break;
    }
}

void ThreadCondition::lockMutex()
{
	int error = pthread_mutex_lock(&_mutex);

    if(error)
	{
		std::string errorString = "Condition embedded pthread mutex lock error. ";
		appendPthreadMutexLockError(error, errorString);
		throw ThreadException(ThreadException::CONDITION_MUTEX_ERROR, error, errorString.c_str());
	}
}

void ThreadCondition::unlockMutex()
{
	int error = pthread_mutex_unlock(&_mutex);

    if(error)
	{
		std::string errorString = "Condition embedded pthread mutex unlock error. ";
		appendPthreadMutexLockError(error, errorString);
		throw ThreadException(ThreadException::CONDITION_MUTEX_ERROR, error, errorString.c_str());
	}
}
