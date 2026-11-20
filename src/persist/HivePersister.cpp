#include "HivePersister.hpp"

#include "HiveLoader.hpp"
#include "HiveSnapshot.hpp"
#include "PersistException.hpp"
#include "json/JsonHiveWriter.hpp"
#include "lmdb/LmdbHiveStore.hpp"
#include "lmdb/lmdbHiveRecords.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphNode.hpp"
#include "../log/log.hpp"
#include "../thread/thread.hpp"

#include <time.h>

namespace
{
	/// How often the writer thread checks the hive's version.
	const unsigned _POLL_INTERVAL_MS = 200;

	/// How long the version has to hold still before a change is written, so that a burst of changes, such as a
	/// builder's whole build, goes in one write.
	const long long _SETTLE_MS = 500;

	/// Longest a change waits to be written while the version keeps changing.
	const long long _MAX_PENDING_MS = 5000;

	long long __nowMs()
	{
		struct timespec now;

		clock_gettime(CLOCK_MONOTONIC, &now);

		return (long long) now.tv_sec * 1000LL + now.tv_nsec / 1000000L;
	}

	void __report(const std::string& hiveName, PersistException& exception)
	{
		std::string message = "Could not store hive '" + hiveName + "': " +
			PersistException::getErrorDescription(exception.getError());

		if(!exception.getDetail().empty()) message += " -- " + exception.getDetail();

		LOG(Logger::ERROR, message)
	}
}

// -- WriterThread --

HivePersister::WriterThread::WriterThread(HivePersister& persister) : _persister(persister)
{
}

HivePersister::WriterThread::~WriterThread()
{
}

void HivePersister::WriterThread::threadEntry()
{
	while(!_getQuit())
	{
		_cond.lockMutex();

		// Re-checked under the lock so a quit signalled via _quitRequested can't be missed.
		if(!_getQuit()) _cond.waitTimeout(_POLL_INTERVAL_MS);

		_cond.unlockMutex();

		if(_getQuit()) break;

		_persister.__poll();
	}
}

void HivePersister::WriterThread::_quitRequested()
{
	_cond.lockMutex();
	_cond.broadcast();
	_cond.unlockMutex();
}

// -- HivePersister --

HivePersister::HivePersister(Handle<LmdbHiveStore> store, Handle<GraphHive> hive, HiveLoader& loader)
	: _store(store), _hive(hive), _recorder(0)
{
	HiveRecorder* recorder = new HiveRecorder(hive, loader);

	_recorder = Handle<HiveRecorder>(recorder);

	// The handle holds the reference; release the implicit construction ref.
	recorder -> decrRef();

	_ownsRecorder = true;

	_hiveName = _hive.getInstance() -> getName();

	// Starting from what is already stored means only what differs from it is ever written, so a hive loaded
	// from the store is not rewritten in full just for having been loaded.
	_written = _store.getInstance() -> readRecords(_hiveName);
}

HivePersister::HivePersister(Handle<LmdbHiveStore> store, Handle<GraphHive> hive, Handle<HiveRecorder> recorder)
	: _store(store), _hive(hive), _recorder(recorder)
{
	_hiveName = _hive.getInstance() -> getName();

	_written = _store.getInstance() -> readRecords(_hiveName);
}

HivePersister::~HivePersister()
{
	// stop() deletes the writer thread; this only covers stop() never having been called, which the hive
	// holding a reference to this as a listener would ordinarily prevent.
	if(_writerThread)
	{
		_writerThread -> stop(true);
		delete _writerThread;
	}
}

void HivePersister::start()
{
	{ SYNC(_lock)

		if(_started || _stopped) return;

		_started = true;
	}

	// Listening first, so that no node joining from here on goes uncaptured until a write happens to reconcile it.
	_recorder.getInstance() -> start();

	_writerThread = new WriterThread(*this);
	_writerThread -> start();
}

void HivePersister::stop()
{
	bool started;

	{ SYNC(_lock)

		if(_stopped) return;

		_stopped = true;
		started = _started;
	}

	if(_writerThread)
	{
		_writerThread -> stop(true);
		delete _writerThread;
		_writerThread = 0;
	}

	if(started && _ownsRecorder) _recorder.getInstance() -> stop();

	try
	{
		flush();
	}
	catch(PersistException& exception)
	{
		__report(_hiveName, exception);
	}
}

void HivePersister::flush()
{
	// Read before anything is captured, so that a change landing part way through leaves the version ahead of
	// what is written and is picked up by the next write.
	unsigned version = _hive.getInstance() -> getVersion();

	// A hive always holds at least one node until it is shut down, after which it holds none. Writing then would
	// store it as having lost every node.
	if(_hive.getInstance() -> getNodeNames().empty()) return;

	std::map<std::string, std::string> records = __buildRecords();

	std::map<std::string, std::string> puts;
	std::vector<std::string> deletes;

	for(const std::pair<const std::string, std::string>& record : records)
	{
		auto writtenIt = _written.find(record.first);

		if(writtenIt == _written.end() || writtenIt -> second != record.second) puts.insert(record);
	}

	for(const std::pair<const std::string, std::string>& record : _written)
	{
		if(records.find(record.first) == records.end()) deletes.push_back(record.first);
	}

	if(!puts.empty() || !deletes.empty()) _store.getInstance() -> writeRecords(_hiveName, puts, deletes);

	_written = records;
	_writtenVersion = version;
}

void HivePersister::__poll()
{
	unsigned version = _hive.getInstance() -> getVersion();

	if(version == _writtenVersion)
	{
		_pendingVersion = 0;
		return;
	}

	long long now = __nowMs();

	if(_pendingVersion == 0) _firstPendingMs = now;

	if(version != _pendingVersion)
	{
		_pendingVersion = version;
		_pendingVersionSeenMs = now;
	}

	if(now - _pendingVersionSeenMs < _SETTLE_MS && now - _firstPendingMs < _MAX_PENDING_MS) return;

	try
	{
		flush();

		_pendingVersion = 0;
	}
	catch(PersistException& exception)
	{
		__report(_hiveName, exception);

		// Restarts the wait, so a write that keeps failing is retried at the settle interval rather than on every
		// poll.
		_pendingVersion = 0;
	}
}

std::map<std::string, std::string> HivePersister::__buildRecords()
{
	HiveSnapshot snapshot = _recorder.getInstance() -> snapshot();

	std::map<std::string, std::string> records;

	std::vector<std::string> nodeOrder;

	for(const HiveNodeDescriptor& descriptor : snapshot.nodes)
	{
		records.emplace(lmdbHiveRecords::nodeKey(descriptor.name), JsonHiveWriter::nodeFragment(descriptor));

		nodeOrder.push_back(descriptor.name);
	}

	std::vector<std::string> surfaceOrder;

	for(const HiveSurfaceDescriptor& surface : snapshot.surfaces)
	{
		records.emplace(lmdbHiveRecords::surfaceKey(surface.name), JsonHiveWriter::surfaceFragment(surface));

		surfaceOrder.push_back(surface.name);
	}

	for(const HiveStrobeDescriptor& emitter : snapshot.strobeEmitters)
	{
		records.emplace(lmdbHiveRecords::strobeEmitterKey(emitter.name),
			JsonHiveWriter::strobeEmitterFragment(emitter.name, emitter.periodMs));
	}

	for(const HiveStrobeDescriptor& surface : snapshot.strobeSurfaces)
	{
		records.emplace(lmdbHiveRecords::strobeSurfaceKey(surface.name),
			JsonHiveWriter::strobeSurfaceFragment(surface.name, surface.periodMs));
	}

	records.emplace(lmdbHiveRecords::META_KEY, lmdbHiveRecords::metaRecord(_hiveName, nodeOrder, surfaceOrder));

	return records;
}
