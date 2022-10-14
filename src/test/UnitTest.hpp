#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include <string>

#define PASS_STRING "Pass"
#define FAIL_STRING "Fail"

/**
 * Encapsulates multiple tests for a particular unit being tested.
 * "Unit Test" and "Test" are different things. "Test" is a sub-test of a Unit Test.
 */
class UnitTest
{
	public:

		virtual ~UnitTest();
		/**
		 * Constructor
		 * @param unitTestName Name given to unit test to identify it.
		 */
		UnitTest(const char* unitTestName);

		/**
		 * Run the unit test.
		 * @returns Unit test overall result.
		 */
		bool run();

		/**
		 * Get the result of running this unit test.
		 * @return true If all tests for unit passed.
		 * @return false If one or more tests for unit passed.
		 */
		bool getResult();

		/**
		 * Add a child unit test to this one. The child will be run when this is run.
		 * @param childUnitTest Unit test to add as child.
		 */
		void addChildUnitTest(UnitTest* childUnitTest);

	protected:

		/**
		 * Run the tests that make up this unit test.
		 * Must be implemented by subclass.
		 */
		virtual void runTests() = 0;

		/**
		 * Hook to allow post processing after tests are run.
		 */
		virtual void postRunTests() {};

		/**
		 * Indicate pass/fail state of individual test that is part of overall unit test.
		 * Can be called multiple times if multiple tests are run per unit.
		 * All tests must pass for unit test to pass.
		 * @param result True for test passing. False if it didn't.
		 * @param resultMessage Message to annotate result.
		 */
		void notifyTestResult(const char* testName, bool result, const char* resultMessage);

		/**
		 * Set the nested level of this unit test.
		 * ie How far down the unit test hierachy it resides.
		 * @param level Nested level to set.
		 */
		void setLevel(int level);

	private:

		/** The tests name. */
		std::string _name;
		/** Whether the unit test, overall, has passed or failed. */
		bool _result;
		/** The nested level of the unit test. Ie where in the tree it resides. */
		int _level;
		/** Pre-calculated string containing indent tabs. */
		std::string _indentTabs;
		/** Child Unit Tests. Just statically allocate it to keep things simple. */
		UnitTest* _childUnitTests[16];
		/** Have to keep track of the number of children. */
		int _numChildUnitTests;

		/** Unit test name must be specified. */
		UnitTest();
		/** Do not allow copying. */
		UnitTest(const UnitTest& copyFrom);
		/** Do not allow copying. */
		UnitTest& operator= (const UnitTest& copyFrom);
};

#endif