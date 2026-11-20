#ifndef HIVE_HOST_H
#define HIVE_HOST_H

#include <functional>
#include <string>
#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../display/GraphHiveSurfaceHtmlMap.hpp"
// Needed in full as the handles held per hosted hive are copied along with it.
#include "../persist/HivePersister.hpp"
#include "../persist/HiveRecorder.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/Handle.hpp"

class GraphHive;
class GraphHiveCollection;
class GraphHiveCollectionHttpMap;
class GraphToolBindingsFactory;
class HiveLoader;
class HttpServerBase;
class LmdbHiveStore;

/**
 * Hosts hives while the program runs: builds each, makes it reachable through the hive collection, keeps it
 * stored when there is a hive database, gives it its model harness and serves its surfaces. A hive can be added
 * and removed at any time, which is what lets the control API create and delete hives as well as the program
 * loading them at startup.
 * @note Thread safe.
 */
class HiveHost
{
	public:

		/**
		 * Builds the agentic harness a hive is run against.
		 * @returns The harness, or an invalid handle to run the hive with no model behind it.
		 */
		typedef std::function<Handle<AgenticHarness>(Handle<GraphHive> hive)> HarnessFactory;

		/**
		 * How each hosted hive's surfaces are served.
		 */
		struct Settings
		{
			/// Threads in each hive's thread pool.
			unsigned numThreads = 2;

			/// Path the collection index is mounted at. Each hive's surfaces are mounted below it.
			std::string indexPath = "/";

			/// How often a surface page polls for changes, in milliseconds.
			unsigned pollIntervalMs = 50;

			/// Lighting model the WebGL views shade with.
			GraphHiveSurfaceHtmlMap::ShadingModel shadingModel = GraphHiveSurfaceHtmlMap::ShadingModel::BLINN_PHONG;

			/// Whether a surface page shows the chat window.
			bool chatEnabled = true;

			/// Capability the chat window asks for.
			AgenticHarness::Capability chatCapability = AgenticHarness::Capability::LOW;
		};

		/**
		 * @param collection Collection every hosted hive is added to. Must outlive this.
		 * @param httpServer Server each hosted hive's surfaces are served from. Must outlive this.
		 * @param collectionMap Index each hosted hive's surfaces are listed on. Must outlive this.
		 * @param store Hive database hosted hives are kept stored in. May be an invalid handle for none.
		 * @param toolBindingsFactory Tool bindings factory given to every hosted hive.
		 * @param harnessFactory Builds the agentic harness of each hosted hive.
		 * @param settings How each hosted hive's surfaces are served.
		 */
		HiveHost(GraphHiveCollection& collection, HttpServerBase& httpServer, GraphHiveCollectionHttpMap& collectionMap,
			Handle<LmdbHiveStore> store, Handle<GraphToolBindingsFactory> toolBindingsFactory,
			HarnessFactory harnessFactory, Settings settings);

		virtual ~HiveHost();

		/**
		 * Build a hive and start hosting it.
		 * @param loader Loader to build the hive from.
		 * @param fromStore Whether the loader reads the hive from the hive database, in which case it is not
		 *        written back to it until it changes.
		 * @param persist Whether to keep the hive stored in the hive database. Ignored when there is none.
		 * @returns The hive.
		 * @throw PersistException If the hive could not be built, or stored.
		 * @throw GraphException(DUPLICATE_HIVE) If a hive of the same name is already in the collection.
		 * @throw ThreadException If keeping the hive stored could not be started.
		 * @note Nothing of the hive is left behind when it throws.
		 */
		Handle<GraphHive> addHive(HiveLoader& loader, bool fromStore, bool persist = true);

		/**
		 * Stop hosting a hive and shut it down.
		 * @param hiveName Name of the hive.
		 * @param purge Whether to also delete the hive from the hive database.
		 * @returns True if the hive was hosted. False if no hive of that name is.
		 * @throw PersistException(LMDB_TXN_FAILED) If the hive could not be deleted from the database. The hive
		 *        has been shut down by then regardless.
		 */
		bool removeHive(std::string hiveName, bool purge);

		/**
		 * Get the recorder of a hosted hive, which says what the hive would have to be rebuilt from.
		 * @param hiveName Name of the hive.
		 * @returns Handle to the recorder. Invalid handle if no hive of that name is hosted.
		 */
		Handle<HiveRecorder> getRecorder(std::string hiveName);

		/**
		 * Get whether a hosted hive is kept stored in the hive database.
		 * @param hiveName Name of the hive.
		 */
		bool getPersisted(std::string hiveName);

		/**
		 * Get whether there is a hive database hives can be kept stored in.
		 */
		bool hasStore();

		/**
		 * Get whether the hive database holds a hive.
		 * @param hiveName Name of the hive.
		 * @returns False if there is no hive database.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be read.
		 */
		bool isStored(std::string hiveName);

		/**
		 * Delete a hive that is not hosted from the hive database.
		 * @param hiveName Name of the hive.
		 * @returns True if the hive was stored and has been deleted. False if it is hosted, is not stored, or there
		 *          is no hive database.
		 * @throw PersistException(LMDB_TXN_FAILED) If the database cannot be written.
		 */
		bool purgeStored(std::string hiveName);

		/**
		 * Get the paths each surface of a hosted hive is served at.
		 * @param hiveName Name of the hive.
		 * @returns Surface name and path pairs, in the order the surfaces were served. Empty if no hive of that
		 *          name is hosted.
		 */
		std::vector<std::pair<std::string, std::string>> getSurfacePaths(std::string hiveName);

		/**
		 * Get the map serving every surface of every hosted hive.
		 * @note Holding a returned handle keeps its map mounted, even after its hive is removed.
		 */
		std::vector<Handle<GraphHiveSurfaceHtmlMap>> getSurfaceMaps();

		/**
		 * Get the path of the first scene surface served of a hosted hive.
		 * @param hiveName Name of the hive.
		 * @returns Path of the surface. Empty if the hive serves no scene surface, or is not hosted.
		 */
		std::string getFirstScenePath(std::string hiveName);

		/**
		 * Stop keeping every hosted hive stored, writing whatever has not yet been written.
		 * @note The hives themselves are left running, for the collection's owner to shut down.
		 */
		void shutdown();

	private:

		// Disable copying.
		HiveHost(const HiveHost& copyFrom);
		HiveHost& operator= (const HiveHost& copyFrom);

		/**
		 * Everything held for one hosted hive.
		 */
		struct HostedHive
		{
			/// The hive.
			Handle<GraphHive> hive{0};

			/// Records what the hive would have to be rebuilt from.
			Handle<HiveRecorder> recorder{0};

			/// Keeps the hive stored. Invalid when it is not kept stored.
			Handle<HivePersister> persister{0};

			/// Maps serving the hive's surfaces.
			std::vector<Handle<GraphHiveSurfaceHtmlMap>> surfaceMaps;

			/// Path of the first scene surface served. Empty if there is none.
			std::string firstScenePath;
		};

		/**
		 * Build and mount a map for every surface of a hive that can be drawn.
		 * @param hosted Hive to serve, whose surfaceMaps and firstScenePath are filled in.
		 */
		void __serveSurfaces(HostedHive& hosted);

		/**
		 * Stop serving and storing a hive that has been taken out of _hives, and shut it down.
		 * @param hosted Hive to dismantle.
		 * @param purge Whether to also delete it from the hive database.
		 */
		void __dismantle(HostedHive& hosted, bool purge);

		/// Collection every hosted hive is added to.
		GraphHiveCollection& _collection;

		/// Server each hosted hive's surfaces are served from.
		HttpServerBase& _httpServer;

		/// Index each hosted hive's surfaces are listed on.
		GraphHiveCollectionHttpMap& _collectionMap;

		/// Hive database hosted hives are kept stored in. Invalid if there is none.
		Handle<LmdbHiveStore> _store;

		/// Tool bindings factory given to every hosted hive.
		Handle<GraphToolBindingsFactory> _toolBindingsFactory;

		/// Builds the agentic harness of each hosted hive.
		HarnessFactory _harnessFactory;

		/// How each hosted hive's surfaces are served.
		Settings _settings;

		/// Every hosted hive. Guarded by _lock.
		std::vector<HostedHive> _hives;

		/// Generic lock.
		ThreadMutex _lock;
};

#endif
