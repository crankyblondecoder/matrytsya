#ifndef GRAPH_NODE_SCHEDULED_ACTION_THREAD_POOL_WORK_UNIT_H
#define GRAPH_NODE_SCHEDULED_ACTION_THREAD_POOL_WORK_UNIT_H

class GraphNode;

#include "../thread/ThreadPoolWorkUnit.hpp"

class GraphNodeScheduledActionThreadPoolWorkUnit : public ThreadPoolWorkUnit
{
	public:

		virtual ~GraphNodeScheduledActionThreadPoolWorkUnit();

		/**
		 * @param node Graph node to process a scheduled action for once this work unit obtains a thread.
		 * @note Will attempt to incrRef node on entry. If that fails, the node is immediately notified of an abort.
		 */
		GraphNodeScheduledActionThreadPoolWorkUnit(GraphNode* node);

		virtual void work();

		virtual void abort();

	private:

		GraphNode* _node;
};

#endif
