#include "TestThread.hpp"
#include "../../thread/ThreadCondition.hpp"
#include "../../thread/ThreadException.hpp"
#include "../UnitTest.hpp"

class ThreadConditionUnitTest : public UnitTest
{
	public:

		ThreadConditionUnitTest() : UnitTest("ThreadConditionUnitTest"){}

	protected:

		virtual void _runTests()
		{
			try
			{
				testThread1.start();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("Thread1 StartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

			try
			{
				testThread2.start();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("Thread2 StartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

			// Get the two test threads to wait on the condition. Signal twice.

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

				if(testThread1.condWaitError)
				{
					_notifyTestResult("First ThreadSignalledOkay", false, testThread1.lastCondExcept -> getSubsystemErrorString().c_str());
					return;
				}

				if(testThread2.condWaitError)
				{
					_notifyTestResult("First ThreadSignalledOkay", false, testThread1.lastCondExcept -> getSubsystemErrorString().c_str());
					return;
				}
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("First ThreadSignalledOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

			// See if one thread woke up.
			if(testThread1.condWaiting && testThread2.condWaiting)
			{
				_notifyTestResult("First ThreadSignalledOkay", false, "Both threads are still waiting on condition.");
				return;
			}
			else if(!testThread1.condWaiting && !testThread2.condWaiting)
			{
				_notifyTestResult("First ThreadSignalledOkay", false, "Both threads stopped waiting on condition.");
				return;
			}
			else
			{
				_notifyTestResult("First ThreadSignalledOkay", true, "");
			}

			// Signal again and make sure the next thread woke up.
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

				if(testThread1.condWaitError)
				{
					_notifyTestResult("Second ThreadSignalledOkay", false, testThread1.lastCondExcept -> getSubsystemErrorString().c_str());
					return;
				}

				if(testThread2.condWaitError)
				{
					_notifyTestResult("Second ThreadSignalledOkay", false, testThread1.lastCondExcept -> getSubsystemErrorString().c_str());
					return;
				}
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("Second ThreadSignalledOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

			if(testThread1.condWaiting || testThread2.condWaiting)
			{
				_notifyTestResult("Second ThreadSignalledOkay", false, "Thread is still waiting on condition.");
			}
			else
			{
				_notifyTestResult("Second ThreadSignalledOkay", true, "");
			}

			// TODO Get the two test threads to wait on the condition. Broadcast once.

			// TODO Get a test thread to wait timeout on the condition. Should come out of wait without having to be signalled.
		}

		virtual void _postRunTests()
		{
			// Make sure no threads are waiting on condition.
			threadWaitCond.broadcast();

			// Stop threads.
			testThread1.signalStop();
			testThread2.signalStop();
		}

	private:

		// Condition for threads to wait on.
		ThreadCondition threadWaitCond;
		// Condition for thread to signal when finished waiting.
		ThreadCondition threadSignalCond;

		TestThread testThread1;
		TestThread testThread2;
};