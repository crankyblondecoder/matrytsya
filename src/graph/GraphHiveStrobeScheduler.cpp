#include "GraphHiveStrobeScheduler.hpp"

#include "GraphHiveSurface.hpp"
#include "nodes/StrobeEmitterNode.hpp"

// Longest the run loop will sleep when nothing is imminently due. Bounds shutdown latency and lets
// the loop periodically re-evaluate even if a wake signal is somehow missed.
#define MAX_STROBE_WAIT_MS 1000

// Shortest sleep the run loop will ever perform. Guards against a zero-length busy spin, noting that
// ThreadCondition::waitTimeout(0) returns immediately rather than blocking.
#define MIN_STROBE_WAIT_MS 1

#define NS_PER_SEC 1000000000L

// -- timespec helpers --

/// True if time a is at or after time b.
static bool timespecGE(const struct timespec& a, const struct timespec& b)
{
	if(a.tv_sec != b.tv_sec) return a.tv_sec > b.tv_sec;

	return a.tv_nsec >= b.tv_nsec;
}

/// Advance t by the given number of nanoseconds.
static void timespecAddNs(struct timespec& t, long ns)
{
	t.tv_sec += ns / NS_PER_SEC;
	t.tv_nsec += ns % NS_PER_SEC;

	if(t.tv_nsec >= NS_PER_SEC)
	{
		t.tv_sec += 1;
		t.tv_nsec -= NS_PER_SEC;
	}
}

/// Whole milliseconds from now until future. Negative if future is already in the past.
static long timespecMsUntil(const struct timespec& future, const struct timespec& now)
{
	long seconds = (long)future.tv_sec - (long)now.tv_sec;
	long nanos = future.tv_nsec - now.tv_nsec;

	return (seconds * 1000L) + (nanos / 1000000L);
}

GraphHiveStrobeScheduler::GraphHiveStrobeScheduler()
{
}

GraphHiveStrobeScheduler::~GraphHiveStrobeScheduler()
{
	// _entries and _surfaceEntries destruct here, releasing each held handle. stop() must have been
	// called already so the run loop is no longer touching either vector.
}

void GraphHiveStrobeScheduler::setEmitter(GraphHandle<StrobeEmitterNode> node, unsigned frequencyHz)
{
	if(!node.isValid() || frequencyHz == 0) return;

	long periodNs = NS_PER_SEC / (long)frequencyHz;

	_cond.lockMutex();

	// Once stop() has been called the run loop may have already exited, so a new entry would never
	// be serviced or released. Checked under the same lock clearEmitters() uses so a registration
	// can never sneak in after a shutdown has cleared everything out.
	if(_getQuit())
	{
		_cond.unlockMutex();
		return;
	}

	struct timespec nextDue;
	clock_gettime(CLOCK_MONOTONIC, &nextDue);
	timespecAddNs(nextDue, periodNs);

	bool updated = false;

	for(StrobeEntry& entry : _entries)
	{
		if(entry.handle == node)
		{
			entry.periodNs = periodNs;
			entry.nextDue = nextDue;
			updated = true;
			break;
		}
	}

	if(!updated)
	{
		StrobeEntry entry{ node, periodNs, nextDue };
		_entries.push_back(entry);
	}

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();
}

void GraphHiveStrobeScheduler::removeEmitter(GraphHandle<StrobeEmitterNode> node)
{
	if(!node.isValid()) return;

	// Hold the removed entry's handle so its final decrRef happens outside the lock.
	GraphHandle<StrobeEmitterNode> removed(0);

	_cond.lockMutex();

	for(std::vector<StrobeEntry>::iterator it = _entries.begin(); it != _entries.end(); ++it)
	{
		if(it -> handle == node)
		{
			// Copy the handle out (incrRef) so erasing the entry can't drop the ref to zero inside
			// the lock.
			removed = it -> handle;
			_entries.erase(it);
			break;
		}
	}

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();

	// 'removed' goes out of scope here, decrRef'ing the node outside the lock.
}

void GraphHiveStrobeScheduler::clearEmitters()
{
	// Swap the entries out so the vector (and each handle's final decrRef) destructs outside the
	// lock, per the "no external calls inside a sync block" rule.
	std::vector<StrobeEntry> removed;

	_cond.lockMutex();

	_entries.swap(removed);

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();

	// 'removed' goes out of scope here, decrRef'ing each node outside the lock.
}

void GraphHiveStrobeScheduler::setSurface(GraphHandle<GraphHiveSurface> surface, unsigned frequencyHz)
{
	if(!surface.isValid() || frequencyHz == 0) return;

	long periodNs = NS_PER_SEC / (long)frequencyHz;

	_cond.lockMutex();

	// Once stop() has been called the run loop may have already exited, so a new entry would never
	// be serviced or released. Checked under the same lock clearSurfaces() uses so a registration
	// can never sneak in after a shutdown has cleared everything out.
	if(_getQuit())
	{
		_cond.unlockMutex();
		return;
	}

	struct timespec nextDue;
	clock_gettime(CLOCK_MONOTONIC, &nextDue);
	timespecAddNs(nextDue, periodNs);

	bool updated = false;

	for(SurfaceEntry& entry : _surfaceEntries)
	{
		if(entry.handle == surface)
		{
			entry.periodNs = periodNs;
			entry.nextDue = nextDue;
			updated = true;
			break;
		}
	}

	if(!updated)
	{
		SurfaceEntry entry{ surface, periodNs, nextDue };
		_surfaceEntries.push_back(entry);
	}

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();
}

void GraphHiveStrobeScheduler::removeSurface(GraphHandle<GraphHiveSurface> surface)
{
	if(!surface.isValid()) return;

	// Hold the removed entry's handle so its final decrRef happens outside the lock.
	GraphHandle<GraphHiveSurface> removed(0);

	_cond.lockMutex();

	for(std::vector<SurfaceEntry>::iterator it = _surfaceEntries.begin(); it != _surfaceEntries.end(); ++it)
	{
		if(it -> handle == surface)
		{
			// Copy the handle out (incrRef) so erasing the entry can't drop the ref to zero inside
			// the lock.
			removed = it -> handle;
			_surfaceEntries.erase(it);
			break;
		}
	}

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();

	// 'removed' goes out of scope here, decrRef'ing the surface outside the lock.
}

void GraphHiveStrobeScheduler::clearSurfaces()
{
	// Swap the entries out so the vector (and each handle's final decrRef) destructs outside the
	// lock, per the "no external calls inside a sync block" rule.
	std::vector<SurfaceEntry> removed;

	_cond.lockMutex();

	_surfaceEntries.swap(removed);

	// Wake the run loop so it re-evaluates the soonest due time.
	_cond.signal();

	_cond.unlockMutex();

	// 'removed' goes out of scope here, decrRef'ing each surface outside the lock.
}

void GraphHiveStrobeScheduler::threadEntry()
{
	while(!_getQuit())
	{
		// Handles keep the due nodes/surfaces alive for the duration of emission; the raw pointers
		// are valid while their handles are held.
		std::vector<GraphHandle<StrobeEmitterNode>> dueEmitters;
		std::vector<GraphHandle<GraphHiveSurface>> dueSurfaces;

		unsigned waitMs = MAX_STROBE_WAIT_MS;

		_cond.lockMutex();

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		for(StrobeEntry& entry : _entries)
		{
			if(timespecGE(now, entry.nextDue))
			{
				dueEmitters.push_back(entry.handle);

				// Advance to the next future emission, skipping any cycles that were missed while
				// the loop was busy or asleep.
				do
				{
					timespecAddNs(entry.nextDue, entry.periodNs);
				}
				while(timespecGE(now, entry.nextDue));
			}

			// Track the soonest upcoming emission so the wait is no longer than necessary.
			long ms = timespecMsUntil(entry.nextDue, now);

			if(ms < 0) ms = 0;

			if(ms < (long)waitMs) waitMs = (unsigned)ms;
		}

		for(SurfaceEntry& entry : _surfaceEntries)
		{
			if(timespecGE(now, entry.nextDue))
			{
				dueSurfaces.push_back(entry.handle);

				// Advance to the next future strobe, skipping any cycles that were missed while the
				// loop was busy or asleep.
				do
				{
					timespecAddNs(entry.nextDue, entry.periodNs);
				}
				while(timespecGE(now, entry.nextDue));
			}

			// Track the soonest upcoming strobe so the wait is no longer than necessary.
			long ms = timespecMsUntil(entry.nextDue, now);

			if(ms < 0) ms = 0;

			if(ms < (long)waitMs) waitMs = (unsigned)ms;
		}

		_cond.unlockMutex();

		// Emit outside the lock, per the "no external calls inside a sync block" rule.
		for(GraphHandle<StrobeEmitterNode> emitter : dueEmitters)
		{
			try
			{
				if(emitter.isValid()) emitter.getInstance() -> emitStrobe();
			}
			catch(...)
			{
				// A single bad emission must not bring down the scheduler thread.
			}
		}

		// Strobe outside the lock, per the "no external calls inside a sync block" rule.
		for(GraphHandle<GraphHiveSurface> surface : dueSurfaces)
		{
			try
			{
				if(surface.isValid()) surface.getInstance() -> strobe();
			}
			catch(...)
			{
				// A single bad strobe must not bring down the scheduler thread.
			}
		}

		// Never allow a zero-length spin wait.
		if(waitMs < MIN_STROBE_WAIT_MS) waitMs = MIN_STROBE_WAIT_MS;

		_cond.lockMutex();

		// Re-check under the lock so a quit signalled via _quitRequested can't be missed.
		if(!_getQuit()) _cond.waitTimeout(waitMs);

		_cond.unlockMutex();
	}
}

void GraphHiveStrobeScheduler::_quitRequested()
{
	// Wake the run loop if it is blocked in waitTimeout so it observes the quit flag promptly.
	_cond.lockMutex();
	_cond.broadcast();
	_cond.unlockMutex();
}
