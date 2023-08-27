#include "../../graph/Graph.hpp"
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


			delete graph;

			stopThreadPool();
		}
};