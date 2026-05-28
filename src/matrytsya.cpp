#include "thread/ThreadPool.hpp"

int main(int argc, char const *argv[])
{
	startThreadPool(6);

	stopThreadPool();

	/* code */
	return 0;
}
