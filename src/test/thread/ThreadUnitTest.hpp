#ifndef THREAD_UNIT_TEST_H
#define THREAD_UNIT_TEST_H

#include <atomic>

#include <gtest/gtest.h>

#include "TestThread.hpp"
#include "../../thread/ThreadException.hpp"
#include "../../thread/ThreadPool.hpp"
#include "../../thread/ThreadPoolWorkUnit.hpp"

namespace
{
	/// Iterations a spin waiting on another thread gives up after, so that a failure cannot hang the suite.
	const unsigned STOPPING_SPIN_LIMIT = 100000;

	/**
	 * Work unit that runs until the worker thread carrying it is asked to stop, standing in for the model
	 * request that this mechanism exists for.
	 */
	class StoppingWorkUnit : public ThreadPoolWorkUnit
	{
		public:

			// The pool deletes the work unit once it has run, so what the test reads is held outside it.
			StoppingWorkUnit(std::atomic<bool>* started, std::atomic<bool>* sawStopping)
				: _started{started}, _sawStopping{sawStopping} {}

			virtual void work() override
			{
				*_started = true;

				for(unsigned spin = 0; spin < STOPPING_SPIN_LIMIT; ++spin)
				{
					if(ThreadBase::currentThreadStopping())
					{
						*_sawStopping = true;

						return;
					}
				}
			}

			virtual void abort() override {}

		private:

			std::atomic<bool>* _started;
			std::atomic<bool>* _sawStopping;
	};
}

class ThreadTest : public ::testing::Test
{
	protected:

		TestThread testThread;
};

TEST_F(ThreadTest, StartsAndStops)
{
	try
	{
		testThread.start();
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	EXPECT_TRUE(testThread.getRunning());

	try
	{
		testThread.stop(true);
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	EXPECT_FALSE(testThread.getRunning());
}

TEST_F(ThreadTest, ReportsStoppingToWorkItCarries)
{
	// The test itself runs on a thread that no ThreadBase ever started, which must be answered rather than
	// faulted on.
	EXPECT_FALSE(ThreadBase::currentThreadStopping());

	try
	{
		testThread.start();
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	try
	{
		testThread.stop(true);
	}
	catch(ThreadException& except)
	{
		FAIL() << except.getSubsystemErrorString();
	}

	EXPECT_FALSE(testThread.stoppingWhileRunning);
	EXPECT_TRUE(testThread.stoppingAfterQuit);

	// The thread has gone, so the answer goes back to being no rather than following a dead pointer.
	EXPECT_FALSE(ThreadBase::currentThreadStopping());
}

TEST_F(ThreadTest, PoolWorkerReportsStoppingToRunningWorkUnit)
{
	std::atomic<bool> started{false};
	std::atomic<bool> sawStopping{false};

	ThreadPool pool(1);

	ASSERT_TRUE(pool.waitOnBecomingActive());
	ASSERT_TRUE(pool.executeWorkUnit(new StoppingWorkUnit(&started, &sawStopping)));

	// The work unit has to be running before the shutdown, or it is aborted from the queue instead and
	// never gets to answer the question this is asking.
	for(unsigned spin = 0; spin < STOPPING_SPIN_LIMIT && !started; ++spin);

	ASSERT_TRUE(started);

	// The pool raises the worker's shutdown flag well ahead of the quit flag, so this is what proves the
	// ThreadPoolWorkThread override rather than the ThreadBase default.
	pool.shutdown();

	EXPECT_TRUE(sawStopping);
}

#endif
