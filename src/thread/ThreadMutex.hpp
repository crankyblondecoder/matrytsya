#ifndef THREAD_MUTEX_H
#define THREAD_MUTEX_H

#include "ThreadMutexPthread.hpp"

// TODO Support for embedded systems that don't have pthreads. For example Pi Pico.
typedef ThreadMutexPthread ThreadMutex;

#endif
