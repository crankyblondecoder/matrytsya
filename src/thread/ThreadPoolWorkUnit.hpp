#ifndef THREAD_POOL_WORK_UNIT_H
#define THREAD_POOL_WORK_UNIT_H

/**
 * Unit of work that is allocated to a thread in a thread pool.
 */
class ThreadPoolWorkUnit
{
    public:

        virtual ~ThreadPoolWorkUnit(){};

		/** Worker thread entry point. */
		virtual void work() = 0;

		/** Worker thread won't be assigned to this work unit. */
		virtual void abort() = 0;

    protected:

    private:
};

#endif
