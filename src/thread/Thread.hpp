#ifndef THREAD_H
#define THREAD_H

#include "ThreadPthread.hpp"

// TODO Support for embedded systems that don't have pthreads. For example Pi Pico.
typedef ThreadPthread Thread;

#endif
