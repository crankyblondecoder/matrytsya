#include "TestThread.hpp"
#include "../../thread/ThreadException.hpp"
#include "../UnitTest.hpp"

class ThreadUnitTest : public UnitTest
{
	public:

		ThreadUnitTest() : UnitTest("ThreadUnitTest"){}

	protected:

		virtual void _runTests()
		{
			// Standard start / stop.

			try
			{
				testThread.start();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("ThreadStartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	_notifyTestResult("ThreadStartedOkay", testThread.getRunning(), "");

			try
			{
				testThread.signalStop();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("ThreadStoppedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	_notifyTestResult("ThreadStoppedOkay", !testThread.getRunning(), "");

			// Re-start, force stop.

			try
			{
				testThread.start();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("ThreadReStartedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	_notifyTestResult("ThreadReStartedOkay - getRunning", testThread.getRunning(), "");

			try
			{
				testThread.forceStop();
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("ThreadForceStoppedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	_notifyTestResult("ThreadForceStoppedOkay - getRunning", !testThread.getRunning(), "");
		}

	private:

		TestThread testThread;
};