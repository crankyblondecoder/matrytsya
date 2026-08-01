#ifndef GRAPH_HIVE_COLLECTION_HTTP_MAP_H
#define GRAPH_HIVE_COLLECTION_HTTP_MAP_H

#include <string>
#include <vector>

#include "../thread/ThreadMutex.hpp"
#include "../util/RefCounted.hpp"
#include "http/HttpRequestHandler.hpp"

class GraphHiveCollection;
class GraphHiveSurfaceMap;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Maps a GraphHiveCollection onto an HTML index, so that a browser arriving with nothing but the server's
 * address can find its way to any surface being served from it. The index is two pages deep: the one this map
 * is mounted at lists the hives the collection holds, and each of those leads to a page listing the surface
 * maps mounted for that hive, which in turn lead to the maps themselves.
 * This map draws the hives from the collection every time it serves the top page, so a hive added to the
 * collection after it was mounted is listed without anything needing to be told. The surface maps offered for a
 * hive are the ones registered here with addSurfaceMap(), since a surface map is mounted on the HTTP server
 * rather than on the hive and only whoever mounted it knows where it went.
 * @note This map is only an index. It answers nothing under a surface map's own path: mounting it at the
 *       server root works because routing is by longest matching prefix, so a request for a mounted surface
 *       map reaches that map and everything left over reaches this one.
 */
class GraphHiveCollectionHttpMap : public RefCounted, public HttpRequestHandler
{
    public:

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param collection Collection this map indexes. Not owned by this, and not reference counted, so it
         *        must outlive this map.
         * @param path Path to mount this index at, e.g. "/". Must end in a separator, as the pages below it
         *        hang directly off it.
         * @note A hive's name and a surface's name are used as URL path segments as they stand, so a name
         *       carrying a character a URL would have to escape cannot be reached through this index.
         */
        GraphHiveCollectionHttpMap(HttpServerBase& httpServer, GraphHiveCollection& collection, std::string path);

        /**
         * Get the path this index is mounted at.
         */
        std::string getPath();

        /**
         * Offer a surface map on the page of the hive it belongs to.
         * @note Only the surface's name and the path the map is mounted at are taken, so this neither owns nor
         *       keeps a reference to the map. A map that goes away while still registered leaves a link behind
         *       that answers as the server does for any unmounted path.
         * @param hiveName Name of the hive the mapped surface belongs to, as the collection knows it.
         * @param surfaceMap Map to offer.
         */
        void addSurfaceMap(std::string hiveName, GraphHiveSurfaceMap& surfaceMap);

        void handleRequest(HttpRequest& request, HttpResponse& response) final;

    protected:

        // Required by ref counting.
        virtual ~GraphHiveCollectionHttpMap();

    private:

        /// A surface map offered on a hive's page.
        struct SurfaceEntry
        {
            /// Name of the mapped surface, which is what the link is labelled with.
            std::string surfaceName;

            /// Path the map is mounted at, which is what the link points at.
            std::string path;
        };

        /// The surface maps offered on one hive's page.
        struct HiveEntry
        {
            /// Name of the hive, as the collection knows it.
            std::string hiveName;

            /// Maps offered on its page, in the order they were registered.
            std::vector<SurfaceEntry> surfaces;
        };

        // Disable copying.
        GraphHiveCollectionHttpMap(const GraphHiveCollectionHttpMap& copyFrom);
        GraphHiveCollectionHttpMap& operator= (const GraphHiveCollectionHttpMap& copyFrom);

        /**
         * Serve the top page, listing the hives the collection currently holds.
         * @param response Response to populate.
         */
        void __serveHiveIndex(HttpResponse& response);

        /**
         * Serve a hive's page, listing the surface maps registered for it.
         * @param hiveName Name of the hive, already matched against the collection.
         * @param response Response to populate.
         */
        void __serveHivePage(const std::string& hiveName, HttpResponse& response);

        /**
         * Get the path this map serves a hive's page at.
         * @param hiveName Name of the hive.
         */
        std::string __hivePath(const std::string& hiveName);

        /**
         * Take a copy of the surface maps registered for a hive.
         * @param hiveName Name of the hive.
         * @returns Its registered maps, or an empty list if none have been registered for it.
         */
        std::vector<SurfaceEntry> __getSurfaceEntries(const std::string& hiveName);

        /// Server this map is registered with.
        HttpServerBase& _httpServer;

        /// Collection this map indexes.
        GraphHiveCollection& _collection;

        /// Path this index is mounted at.
        std::string _path;

        /// Surface maps registered per hive. Guarded by _lock, as they are registered from whichever thread
        /// mounted them while the pages listing them are served from the HTTP server's own thread.
        std::vector<HiveEntry> _hives;

        /// Guards _hives.
        ThreadMutex _lock;
};

#endif
