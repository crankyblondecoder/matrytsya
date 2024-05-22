#include "ThreadConditionUnitTest.hpp"
#include "ThreadUnitTest.hpp"
#include "../UnitTest.hpp"

#ifndef THREAD_MODULE_UNIT_TESTS_H
#define THREAD_MODULE_UNIT_TESTS_H

class ThreadModuleUnitTests : public UnitTest
{
	public:

		// Add all top level unit tests as children of this.

		ThreadModuleUnitTests() : UnitTest("ThreadModuleUnitTests")
		{
			addChildUnitTest(&threadUnitTest);
			addChildUnitTest(&threadConditionUnitTest);
		}

	protected:

		virtual void _runTests() {};

	private:

		ThreadUnitTest threadUnitTest;
		ThreadConditionUnitTest threadConditionUnitTest;
};

#endif