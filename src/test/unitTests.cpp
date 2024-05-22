#include <iostream>
#include <signal.h>

#include "./ping/PingTest.hpp"
#include "./thread/ThreadModuleUnitTests.hpp"

unsigned numTests = 0;

UnitTest* unitTests[16];

void handleCtrlC(int sigNum)
{
	cout << "\n** tests interrupted **\n\n";

	for(unsigned index = 0; index < numTests; index++)
	{
		unitTests[index] -> handleCtrlC();
	}

	cout << "\n";

	exit(0);
}

int main()
{
	signal(SIGINT, handleCtrlC);

	// Thread module.
	ThreadModuleUnitTests threadModuleUnitTests;
	unitTests[0] = &threadModuleUnitTests;

	// Ping Test.
	PingTest pingTest;
	unitTests[1] = &pingTest;

	numTests = 2;

	threadModuleUnitTests.run();
	pingTest.run();
}

