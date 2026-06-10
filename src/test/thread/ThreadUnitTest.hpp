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
				testThread.stop(true);
			}
			catch(ThreadException& except)
			{
				_notifyTestResult("ThreadStoppedOkay", false, except.getSubsystemErrorString().c_str());
				return;
			}

        	_notifyTestResult("ThreadStoppedOkay", !testThread.getRunning(), "");
		}

	private:

		TestThread testThread;
};
