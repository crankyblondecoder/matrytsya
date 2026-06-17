#ifndef THREAD_CONDITION_UNIT_TEST_H
#define THREAD_CONDITION_UNIT_TEST_H

#include <gtest/gtest.h>

#include "TestThread.hpp"
#include "../../thread/ThreadCondition.hpp"
#include "../../thread/ThreadException.hpp"

class ThreadConditionTest : public ::testing::Test
{
	protected:

		void TearDown() override
		{
			// Make sure no threads are waiting on condition.
			threadWaitCond.broadcast();

			// Stop threads.
			testThread1.stop(true);
			testThread2.stop(true);
		}

		// Condition for threads to wait on.
		ThreadCondition threadWaitCond;
		// Condition for thread to signal when finished waiting.
		ThreadCondition threadSignalCond;

		TestThread testThread1;
		TestThread testThread2;
};

TEST_F(ThreadConditionTest, FirstThreadSignalledOkay)
{
	try { testThread1.start(); }
	catch(ThreadException& except) { FAIL() << except.getSubsystemErrorString(); }

	try { testThread2.start(); }
	catch(ThreadException& except) { FAIL() << except.getSubsystemErrorString(); }

	try
	{
		// Stop any thread from signalling this before it is ready.
		threadSignalCond.lockMutex();

		testThread1.waitOnCond(&threadWaitCond, &threadSignalCond);
		testThread2.waitOnCond(&threadWaitCond, &threadSignalCond);

		// Use the conditions mutex to wait on test thread to wait on condition.
		threadWaitCond.lockMutex();
		threadWaitCond.signal();
		threadWaitCond.unlockMutex();

		// Wait for one of the threads to signal that it is no longer waiting on the condition.
		threadSignalCond.waitTimeout(2000);
		threadSignalCond.unlockMutex();
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	if(testThread1.condWaitError)
	{
		FAIL() << testThread1.lastCondExcept -> getSubsystemErrorString();
	}

	if(testThread2.condWaitError)
	{
		FAIL() << testThread2.lastCondExcept -> getSubsystemErrorString();
	}

	// See if exactly one thread woke up.
	bool t1waiting = testThread1.condWaiting;
	bool t2waiting = testThread2.condWaiting;

	if(t1waiting && t2waiting)
	{
		FAIL() << "Both threads are still waiting on condition.";
	}
	else if(!t1waiting && !t2waiting)
	{
		FAIL() << "Both threads stopped waiting on condition.";
	}
}

TEST_F(ThreadConditionTest, SecondThreadSignalledOkay)
{
	try { testThread1.start(); }
	catch(ThreadException& except) { FAIL() << except.getSubsystemErrorString(); }

	try { testThread2.start(); }
	catch(ThreadException& except) { FAIL() << except.getSubsystemErrorString(); }

	// First signal.
	try
	{
		// Stop any thread from signalling this before it is ready.
		threadSignalCond.lockMutex();

		testThread1.waitOnCond(&threadWaitCond, &threadSignalCond);
		testThread2.waitOnCond(&threadWaitCond, &threadSignalCond);

		// Use the conditions mutex to wait on test thread to wait on condition.
		threadWaitCond.lockMutex();
		threadWaitCond.signal();
		threadWaitCond.unlockMutex();

		// Wait for one of the threads to signal that it is no longer waiting on the condition.
		threadSignalCond.waitTimeout(2000);
		threadSignalCond.unlockMutex();
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	// Second signal.
	try
	{
		// Stop any thread from signalling this before it is ready.
		threadSignalCond.lockMutex();

		// Use the conditions mutex to wait on test thread to wait on condition.
		threadWaitCond.lockMutex();
		threadWaitCond.signal();
		threadWaitCond.unlockMutex();

		// Wait for the final thread to signal that it is no longer waiting on the condition.
		threadSignalCond.waitTimeout(2000);
		threadSignalCond.unlockMutex();
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	if(testThread1.condWaitError)
	{
		FAIL() << testThread1.lastCondExcept -> getSubsystemErrorString();
	}

	if(testThread2.condWaitError)
	{
		FAIL() << testThread2.lastCondExcept -> getSubsystemErrorString();
	}

	EXPECT_FALSE(testThread1.condWaiting) << "Thread1 is still waiting on condition.";
	EXPECT_FALSE(testThread2.condWaiting) << "Thread2 is still waiting on condition.";
}

// TODO: Test that broadcast wakes all waiting threads.

// TODO: Test that waitTimeout comes out of wait without being signalled.

#endif
