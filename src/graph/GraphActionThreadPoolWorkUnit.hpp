#ifndef GRAPH_ACTION_THREAD_POOL_WORK_UNIT_H
#define GRAPH_ACTION_THREAD_POOL_WORK_UNIT_H

class GraphAction;

#include "../thread/ThreadPoolWorkUnit.hpp"

class GraphActionThreadPoolWorkUnit : public ThreadPoolWorkUnit
{
	public:

		virtual ~GraphActionThreadPoolWorkUnit();

		/**
		 * @param graphAction Graph action to invoke once work unit obtains a thread. Expected to have been incrRef on action
		 *        before entry.
		 */
		GraphActionThreadPoolWorkUnit(GraphAction* graphAction);

		virtual void work();

		virtual void abort();

	private:

		GraphAction* _graphAction;
};

#endif