#include "TestThread.hpp"
#include "../../thread/ThreadException.hpp"
#include "../UnitTest.hpp"

class ThreadUnitTest : public UnitTest
{
	public:

		ThreadUnitTest() : UnitTest("ThreadUnitTest"){}

	protected:

		virtual void runTests()
		{
			// Standard start / stop.

			try
			{
				testThread.start();
			}
			catch(ThreadException& except)
			{
				notifyTestResult("ThreadStartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	notifyTestResult("ThreadStartedOkay", testThread.getRunning(), "");

			try
			{
				testThread.signalStop();
			}
			catch(ThreadException& except)
			{
				notifyTestResult("ThreadStoppedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	notifyTestResult("ThreadStoppedOkay", !testThread.getRunning(), "");

			// Re-start, force stop.

			try
			{
				testThread.start();
			}
			catch(ThreadException& except)
			{
				notifyTestResult("ThreadReStartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	notifyTestResult("ThreadReStartedOkay - getRunning", testThread.getRunning(), "");

			try
			{
				testThread.forceStop();
			}
			catch(ThreadException& except)
			{
				notifyTestResult("ThreadForceStoppedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	notifyTestResult("ThreadForceStoppedOkay - getRunning", !testThread.getRunning(), "");
		}

	private:

		TestThread testThread;
};