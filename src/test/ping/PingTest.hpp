#include "../../graph/GraphHive.hpp"
#include "../../graph/graphEdgeFlagRegister.hpp"
#include "../../graph/GraphNodeHandle.hpp"
#include "../../graph/nodes/TestNode.hpp"
#include "../../thread/ThreadPool.hpp"
#include "../UnitTest.hpp"

#include <iostream>
using namespace std;

class PingTest : public UnitTest
{
	public:

		PingTest() : UnitTest("PingTest")
		{
		}

		void handleCtrlC()
		{
			enumerateThreadPool(0);
		}

	protected:

		virtual void _runTests()
		{
			__actionEnergyRundownTest();
		}

	private:

		void __actionEnergyRundownTest()
		{
			// This creates a hive and runs an action that should cycle until it runs out of energy.

			cout << "\nAction energy rundown test.\n";

			startThreadPool(2);

			GraphHive* hive = new GraphHive();

			// Build a network of nodes and apply ping test action.
			// The nodes must _not_ be allocated on the stack because of auto-delete once de-reff'd.

			TestNode* testNode1 = new TestNode(*hive);
			TestNode* testNode2 = new TestNode(*hive);
			TestNode* testNode3 = new TestNode(*hive);

			GraphNodeHandle nodeHandle1(testNode1);
			GraphNodeHandle nodeHandle2(testNode2);
			GraphNodeHandle nodeHandle3(testNode3);

			// Deliberately create a cycle.
			testNode1 -> createEdge(nodeHandle2);
			testNode2 -> createEdge(nodeHandle3);
			testNode3 -> createEdge(nodeHandle1);

			// Run ping action.
			testNode1 -> emitPing();

			// TODO How to wait for action to complete?

			delete hive;

			stopThreadPool();
		}

		void __waitUntilActionCompleteTest()
		{

		}
};
