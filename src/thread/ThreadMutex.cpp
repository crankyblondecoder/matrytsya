#include <errno.h>

#include "pthread_errors.hpp"
#include "ThreadException.hpp"
#include "ThreadMutex.hpp"

ThreadMutex::~ThreadMutex()
{
    // Do not handle any errors because at this point there is nothing that can be done about it.
    pthread_mutex_destroy(&_mutex);
}

ThreadMutex::ThreadMutex()
{
    int error = pthread_mutex_init(&_mutex, NULL);

    switch(error)
    {
        case EAGAIN:

            throw ThreadException(ThreadException::MUTEX_RESOURCES_EXHAUSTED, error, PTHREAD_MUTEX_INIT_EAGAIN);
            break;

        case ENOMEM:

            throw ThreadException(ThreadException::MUTEX_RESOURCES_EXHAUSTED, error, PTHREAD_MUTEX_INIT_ENOMEM);
            break;

        case EPERM:

            throw ThreadException(ThreadException::MUTEX_INSUFFICIENT_PRIVILEGES, error, PTHREAD_MUTEX_INIT_EPERM);
            break;

        case EBUSY:

            throw ThreadException(ThreadException::MUTEX_BUSY, error, PTHREAD_MUTEX_INIT_EBUSY);
            break;

        case EINVAL:

            throw ThreadException(ThreadException::MUTEX_INCORRECT_PARAMETER, error, PTHREAD_MUTEX_INIT_EINVAL);
            break;

        default:

            if(error) throw ThreadException(ThreadException::MUTEX_GENERAL_ERROR, error);
            break;
    }
}

bool ThreadMutex::lock()
{
    int error = pthread_mutex_lock(&_mutex);

    switch(error)
    {
        case EINVAL:

            throw ThreadException(ThreadException::MUTEX_PRIORITY_CONFLICT, error, PTHREAD_MUTEX_LOCK_EINVAL);
            break;

        case EAGAIN:

            throw ThreadException(ThreadException::MUTEX_RESOURCES_EXHAUSTED, error, PTHREAD_MUTEX_LOCK_EAGAIN);
            break;

        case EDEADLK:

            throw ThreadException(ThreadException::MUTEX_BUSY, error, PTHREAD_MUTEX_LOCK_EDEADLK);
            break;

        default:

            if(error) throw ThreadException(ThreadException::MUTEX_GENERAL_ERROR, error);
            break;
    }

    return true;
}

bool ThreadMutex::tryLock()
{
    int error = pthread_mutex_trylock(&_mutex);

    if(error == EBUSY) return false;

    switch(error)
    {
        case EINVAL:

            throw ThreadException(ThreadException::MUTEX_PRIORITY_CONFLICT, error,
                "EINVAL The value specified by mutex does not refer  to  an  initialized mutex object.");
            break;

        case EAGAIN:

            throw ThreadException(ThreadException::MUTEX_RESOURCES_EXHAUSTED, error,
                "EAGAIN The mutex could not be acquired because the maximum number of recursive locks for mutex has been exceeded.");
            break;

        default:

            if(error) throw ThreadException(ThreadException::MUTEX_GENERAL_ERROR, error);
            break;
    }

    return true;
}

bool ThreadMutex::unlock()
{
    int error = pthread_mutex_unlock(&_mutex);

    switch(error)
    {
        case EINVAL:

            throw ThreadException(ThreadException::MUTEX_PRIORITY_CONFLICT, error, PTHREAD_MUTEX_UNLOCK_EINVAL);
            break;

        case EAGAIN:

            throw ThreadException(ThreadException::MUTEX_RESOURCES_EXHAUSTED, error, PTHREAD_MUTEX_UNLOCK_EAGAIN);
            break;

        case EPERM:

            throw ThreadException(ThreadException::MUTEX_INSUFFICIENT_PRIVILEGES, error, PTHREAD_MUTEX_UNLOCK_EPERM);
            break;

        default:

            if(error) throw ThreadException(ThreadException::MUTEX_GENERAL_ERROR, error);
            break;
    }

    return true;
}
