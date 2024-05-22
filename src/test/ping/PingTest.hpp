#include "../../graph/Graph.hpp"
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
			_harshShutdownTest_emitPingFinished = false;
			_harshShutdownTest_deleteGraphFinished = false;
			_harshShutdownTest_stopThreadPoolFinished = false;
		}

		void handleCtrlC()
		{
			cout << "_harshShutdownTest_emitPingFinished: " << _harshShutdownTest_emitPingFinished << "\n";
			cout << "_harshShutdownTest_deleteGraphFinished: " << _harshShutdownTest_deleteGraphFinished << "\n";
			cout << "_harshShutdownTest_stopThreadPoolFinished: " << _harshShutdownTest_stopThreadPoolFinished << "\n";

			cout << "thread pool here: " << getThreadPoolHere() << "\n";

			// TODO ... Dump mutex stats
		}

	protected:

		virtual void _runTests()
		{
			__harshShutdownTest();
		}

	private:

		bool _harshShutdownTest_emitPingFinished;
		bool _harshShutdownTest_deleteGraphFinished;
		bool _harshShutdownTest_stopThreadPoolFinished;

		void __harshShutdownTest()
		{
			// This creates a graph then immediately shuts it down before an action can complete.

			startThreadPool(2);

			Graph* graph = new Graph();

			// Build a network of nodes and apply ping test action.
			// The nodes must _not_ be allocated on the stack because of auto-delete once de-reff'd.

			TestNode* testNode1 = new TestNode(graph);
			TestNode* testNode2 = new TestNode(graph);
			TestNode* testNode3 = new TestNode(graph);

			GraphNodeHandle nodeHandle1(testNode1);
			GraphNodeHandle nodeHandle2(testNode2);
			GraphNodeHandle nodeHandle3(testNode3);

			// Deliberately create a cycle.
			testNode1 -> formEdgeTo(nodeHandle2, TEST_GRAPH_EDGE);
			testNode2 -> formEdgeTo(nodeHandle3, TEST_GRAPH_EDGE);
			testNode3 -> formEdgeTo(nodeHandle1, TEST_GRAPH_EDGE);

			// Run ping action and deli
			testNode1 -> emitPing();

			_harshShutdownTest_emitPingFinished = true;

			delete graph;

			_harshShutdownTest_deleteGraphFinished = true;

			stopThreadPool();

			_harshShutdownTest_stopThreadPoolFinished = true;
		}
};