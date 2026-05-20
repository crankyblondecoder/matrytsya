#ifndef THREAD_CONDITION_H
#define THREAD_CONDITION_H

#include "ThreadConditionPthread.hpp"

// TODO Support for embedded systems that don't have pthreads. For example Pi Pico.
typedef ThreadConditionPthread ThreadCondition;

#endif
