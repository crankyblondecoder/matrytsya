#include "HiveHost.hpp"

#include "../display/GraphHiveCollectionHttpMap.hpp"
#include "../display/GraphHiveGraphViewSurfaceWebglMap.hpp"
#include "../display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "../display/http/HttpServerBase.hpp"
#include "../graph/GraphException.hpp"
#include "../graph/GraphHive.hpp"
#include "../graph/GraphHiveCollection.hpp"
#include "../graph/GraphHiveGraphViewSurface.hpp"
#include "../graph/GraphHiveSceneSurface.hpp"
#include "../graph/GraphToolBindingsFactory.hpp"
#include "../persist/HiveBuilder.hpp"
#include "../persist/HiveLoader.hpp"
#include "../persist/HivePersister.hpp"
#include "../persist/HiveRecorder.hpp"
#include "../persist/PersistException.hpp"
#include "../persist/lmdb/LmdbHiveStore.hpp"
#include "../thread/thread.hpp"

HiveHost::HiveHost(GraphHiveCollection& collection, HttpServerBase& httpServer,
	GraphHiveCollectionHttpMap& collectionMap, Handle<LmdbHiveStore> store,
	Handle<GraphToolBindingsFactory> toolBindingsFactory, HarnessFactory harnessFactory, Settings settings) :
	_collection{collection}, _httpServer{httpServer}, _collectionMap{collectionMap}, _store{store},
	_toolBindingsFactory{toolBindingsFactory}, _harnessFactory{harnessFactory}, _settings{settings}
{
}

HiveHost::~HiveHost()
{
	shutdown();
}

Handle<GraphHive> HiveHost::addHive(HiveLoader& loader, bool fromStore, bool persist)
{
	GraphHive* hive = HiveBuilder::build(loader, _settings.numThreads);

	// The construction reference is not released here: the hive drops it itself when shut down.
	HostedHive hosted{Handle<GraphHive>(hive), Handle<HiveRecorder>(0), Handle<HivePersister>(0), {}, ""};

	// Both directions of the association are needed: the collection serves the hive up by name, and the hive
	// reaches back through the collection to teleport an action to a node in one of the others.
	try
	{
		_collection.addHive(hosted.hive);
	}
	catch(...)
	{
		hive -> shutdown();
		throw;
	}

	// Whether the hive was written to the store by this call, and so is deleted from it again if this fails.
	bool storedHere = false;

	try
	{
		HiveRecorder* recorder = new HiveRecorder(hosted.hive, loader);

		hosted.recorder = Handle<HiveRecorder>(recorder);

		recorder -> decrRef();

		recorder -> start();

		if(_store.isValid() && persist)
		{
			HivePersister* persister = new HivePersister(_store, hosted.hive, hosted.recorder);

			hosted.persister = Handle<HivePersister>(persister);

			persister -> decrRef();

			// A hive not read from the store is stored straight away, so that it is in the database from now on
			// whether or not it ever changes.
			if(!fromStore)
			{
				persister -> flush();

				storedHere = true;
			}

			persister -> start();
		}

		hive -> setHiveCollection(&_collection);

		hive -> setToolBindingsFactory(_toolBindingsFactory);

		// Set before this hive's surfaces are served, so that the first chat request cannot arrive without a model
		// behind it.
		if(_harnessFactory) hive -> setAgenticHarness(_harnessFactory(hosted.hive));

		__serveSurfaces(hosted);

		for(Handle<GraphHiveSurfaceHtmlMap>& surfaceMap : hosted.surfaceMaps)
		{
			_collectionMap.addSurfaceMap(hive -> getName(), *surfaceMap.getInstance());
		}
	}
	catch(...)
	{
		// Taken apart as a removal would, so that a hive that fails part way leaves nothing of itself behind: not
		// in the collection, not served, and not in the store unless it was there before this.
		// A failure to delete what was stored is not reported over the failure that caused it.
		try
		{
			__dismantle(hosted, storedHere);
		}
		catch(...)
		{
		}

		throw;
	}

	{ SYNC(_lock)

		_hives.push_back(hosted);
	}

	return hosted.hive;
}

bool HiveHost::removeHive(std::string hiveName, bool purge)
{
	HostedHive hosted;
	bool found = false;

	{ SYNC(_lock)

		for(std::vector<HostedHive>::iterator iter = _hives.begin(); iter != _hives.end(); iter++)
		{
			if(iter -> hive.getInstance() -> getName() == hiveName)
			{
				hosted = *iter;
				found = true;

				_hives.erase(iter);
				break;
			}
		}
	}

	if(!found) return false;

	__dismantle(hosted, purge);

	return true;
}

Handle<HiveRecorder> HiveHost::getRecorder(std::string hiveName)
{
	{ SYNC(_lock)

		for(HostedHive& hosted : _hives)
		{
			if(hosted.recorder.getInstance() -> getHiveName() == hiveName) return hosted.recorder;
		}
	}

	return Handle<HiveRecorder>(0);
}

bool HiveHost::getPersisted(std::string hiveName)
{
	{ SYNC(_lock)

		for(HostedHive& hosted : _hives)
		{
			if(hosted.recorder.getInstance() -> getHiveName() == hiveName) return hosted.persister.isValid();
		}
	}

	return false;
}

bool HiveHost::hasStore()
{
	return _store.isValid();
}

bool HiveHost::isStored(std::string hiveName)
{
	return _store.isValid() && _store.getInstance() -> hasHive(hiveName);
}

bool HiveHost::purgeStored(std::string hiveName)
{
	if(!_store.isValid() || getRecorder(hiveName).isValid()) return false;

	return _store.getInstance() -> deleteHive(hiveName);
}

std::vector<std::pair<std::string, std::string>> HiveHost::getSurfacePaths(std::string hiveName)
{
	std::vector<Handle<GraphHiveSurfaceHtmlMap>> surfaceMaps;

	{ SYNC(_lock)

		for(HostedHive& hosted : _hives)
		{
			if(hosted.recorder.getInstance() -> getHiveName() == hiveName) surfaceMaps = hosted.surfaceMaps;
		}
	}

	std::vector<std::pair<std::string, std::string>> paths;

	// Asked of the maps outside the lock, as each reaches into the surface it serves.
	for(Handle<GraphHiveSurfaceHtmlMap>& surfaceMap : surfaceMaps)
	{
		paths.push_back(std::make_pair(surfaceMap.getInstance() -> getSurfaceName(), surfaceMap.getInstance() -> getPath()));
	}

	return paths;
}

std::vector<Handle<GraphHiveSurfaceHtmlMap>> HiveHost::getSurfaceMaps()
{
	std::vector<Handle<GraphHiveSurfaceHtmlMap>> surfaceMaps;

	{ SYNC(_lock)

		for(HostedHive& hosted : _hives)
		{
			surfaceMaps.insert(surfaceMaps.end(), hosted.surfaceMaps.begin(), hosted.surfaceMaps.end());
		}
	}

	return surfaceMaps;
}

std::string HiveHost::getFirstScenePath(std::string hiveName)
{
	{ SYNC(_lock)

		for(HostedHive& hosted : _hives)
		{
			if(hosted.recorder.getInstance() -> getHiveName() == hiveName) return hosted.firstScenePath;
		}
	}

	return "";
}

void HiveHost::shutdown()
{
	std::vector<HostedHive> hives;

	{ SYNC(_lock)

		hives = _hives;
	}

	// Stopping either twice is harmless, so a later removeHive() or a second shutdown() is too.
	for(HostedHive& hosted : hives)
	{
		if(hosted.persister.isValid()) hosted.persister.getInstance() -> stop();

		hosted.recorder.getInstance() -> stop();
	}
}

void HiveHost::__serveSurfaces(HostedHive& hosted)
{
	GraphHive* hive = hosted.hive.getInstance();

	// Worked out before any map is built, since a scene surface has to be told where this hive's structure is
	// when its map is made, and the surface holding it may well be listed after it. Empty when the hive mounts no
	// graph view surface, which is what leaves a scene page offering no such link.
	std::string structurePath;

	for(std::string surfaceName : hive -> getSurfaceNames())
	{
		if(!hive -> getGraphViewSurface(surfaceName).isValid()) continue;

		structurePath = _settings.indexPath + hive -> getName() + "/" + surfaceName + "/";

		break;
	}

	for(std::string surfaceName : hive -> getSurfaceNames())
	{
		// Nested under the hive's own page, so that the index reads as one tree rather than a listing pointing off
		// somewhere else.
		std::string surfacePath = _settings.indexPath + hive -> getName() + "/" + surfaceName + "/";

		Handle<GraphHiveSceneSurface> sceneSurfaceHandle = hive -> getSceneSurface(surfaceName);
		Handle<GraphHiveGraphViewSurface> graphViewSurfaceHandle = hive -> getGraphViewSurface(surfaceName);

		GraphHiveSurfaceHtmlMap* surfaceMap = 0;

		if(sceneSurfaceHandle.isValid())
		{
			surfaceMap = new GraphHiveSceneSurfaceWebglMap(_httpServer, *sceneSurfaceHandle.getInstance(), surfacePath);

			// Only a scene page is offered the link. The structure page is the one it would lead to.
			surfaceMap -> setStructurePath(structurePath);

			if(hosted.firstScenePath.empty()) hosted.firstScenePath = surfacePath;
		}
		else if(graphViewSurfaceHandle.isValid())
		{
			surfaceMap = new GraphHiveGraphViewSurfaceWebglMap(_httpServer, *graphViewSurfaceHandle.getInstance(),
				surfacePath);
		}
		else
		{
			// A surface of a kind nothing here knows how to draw. Left out of the index rather than fatal, as the
			// hive is still perfectly serviceable without it being viewable.
			continue;
		}

		Handle<GraphHiveSurfaceHtmlMap> surfaceMapHandle(surfaceMap);

		// The handle holds the reference; release the implicit construction ref, so that dropping the handle when
		// the hive is removed unmounts the map.
		surfaceMap -> decrRef();

		surfaceMap -> setPollInterval(_settings.pollIntervalMs);

		surfaceMap -> setShadingModel(_settings.shadingModel);

		surfaceMap -> setChatEnabled(_settings.chatEnabled);

		surfaceMap -> setIndexPath(_collectionMap.getListingPath());

		// The map asks for MEDIUM by default, which a harness need not assign any model to.
		surfaceMap -> setChatCapability(_settings.chatCapability);

		hosted.surfaceMaps.push_back(surfaceMapHandle);
	}
}

void HiveHost::__dismantle(HostedHive& hosted, bool purge)
{
	GraphHive* hive = hosted.hive.getInstance();
	std::string hiveName = hive -> getName();

	// Unlisted and unmounted first, so that nothing new reaches the hive while it is taken apart. A request
	// already inside one of its maps holds that map, and the map its surface, until the request is answered.
	_collectionMap.removeHive(hiveName);

	// Unmounted explicitly, as the surface each serves holds it as a listener, so dropping the handles alone would
	// leave it mounted.
	for(Handle<GraphHiveSurfaceHtmlMap>& surfaceMap : hosted.surfaceMaps) surfaceMap.getInstance() -> unmount();

	hosted.surfaceMaps.clear();

	// Stopped before the hive is shut down, as the hive holds the recorder as a listener until then, and so that
	// the last of its changes is written.
	if(hosted.persister.isValid()) hosted.persister.getInstance() -> stop();

	if(hosted.recorder.isValid()) hosted.recorder.getInstance() -> stop();

	_collection.removeHive(hiveName);

	hive -> setAgenticHarness(Handle<AgenticHarness>(0));

	hive -> shutdown();

	if(purge && _store.isValid()) _store.getInstance() -> deleteHive(hiveName);
}
