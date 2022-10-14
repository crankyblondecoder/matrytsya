#ifndef THREAD_MODULE_H
#define THREAD_MODULE_H

// Synchronise on a particular mutex.
// As a general rule. Never execute functions inside sync blocks. Only ever read/write thread affected data.
#define SYNC(mutex) ThreadLock __lock(&mutex);

#include "Thread.hpp"
#include "ThreadCondition.hpp"
#include "ThreadLock.hpp"
#include "ThreadMutex.hpp"

#endif
