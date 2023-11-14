#include "../../graph/Graph.hpp"
#include "../../graph/graphEdgeFlagRegister.hpp"
#include "../../graph/GraphNodeHandle.hpp"
#include "../../graph/nodes/TestNode.hpp"
#include "../../thread/ThreadPool.hpp"
#include "../UnitTest.hpp"

class PingTest : public UnitTest
{
	public:

		PingTest() : UnitTest("PingTest"){}

	protected:

		virtual void runTests()
		{
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

			// Run ping action.
			testNode1 -> emitPing();

			delete graph;

			stopThreadPool();
		}
};