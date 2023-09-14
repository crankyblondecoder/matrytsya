#include "../../graph/Graph.hpp"
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
			// The nodes must _not_ be allocated on the stack because of auto-delete once de-reffed.

			new TestNode(graph);
			new TestNode(graph);
			new TestNode(graph);

			delete graph;

			stopThreadPool();
		}
};