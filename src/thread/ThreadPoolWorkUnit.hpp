#ifndef THREAD_POOL_WORK_UNIT_H
#define THREAD_POOL_WORK_UNIT_H

/**
 * Unit of work that is allocated to a thread in a thread pool.
 */
class ThreadPoolWorkUnit
{
    public:

        virtual ~ThreadPoolWorkUnit();

		virtual void work() = 0;

    protected:

    private:
};

#endif
