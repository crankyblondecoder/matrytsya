#ifndef THREAD_UNIT_TEST_H
#define THREAD_UNIT_TEST_H

#include <gtest/gtest.h>

#include "TestThread.hpp"
#include "../../thread/ThreadException.hpp"

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

#endif
