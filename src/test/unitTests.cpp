#include "./PingTest.hpp"
#include "./thread/ThreadModuleUnitTests.hpp"

int main()
{
	// Thread module.
	ThreadModuleUnitTests threadModuleUnitTests;
	threadModuleUnitTests.run();

	// Ping Test.
	PingTest pingTest;
	pingTest.run();
}