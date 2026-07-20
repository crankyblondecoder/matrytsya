#ifndef GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H
#define GRAPH_HIVE_SCENE_SURFACE_WEBGL_MAP_H

#include <string>

#include "../graph/GraphHandle.hpp"
#include "../graph/GraphHiveSurfaceListener.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../util/EventListener.hpp"
#include "GraphHiveSurfaceHttpMap.hpp"

class GraphHiveSceneSurface;
class GraphHiveSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Maps a GraphHiveSceneSurface onto a WebGL based HTML interface so that its geometry can be viewed in a browser.
 * This map binds to a single scene surface for its whole lifetime and listens for that surface's changed event
 * to know when browsers viewing its page should pick up new data, without needing to be reloaded.
 */
class GraphHiveSceneSurfaceWebglMap : public GraphHiveSurfaceHttpMap, private EventListener<GraphHiveSurfaceListener>,
	private GraphHiveSurfaceListener
{
    public:

        virtual ~GraphHiveSceneSurfaceWebglMap();

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Scene surface this map binds to for its whole lifetime. This map keeps its own
         *        reference to it (see GraphHandle), released once this map is destroyed.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/scene".
         */
        GraphHiveSceneSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveSceneSurface& surface, std::string path);

        /**
         * Set how often browsers viewing this map's page poll to check for changes.
         * @param pollIntervalMs Poll interval in milliseconds. Defaults to 1000.
         */
        void setPollInterval(unsigned pollIntervalMs);

    protected:

        void _renderPage(HttpResponse& response) override;

        void _serveData(HttpRequest& request, HttpResponse& response) override;

    private:

        // Disable copying.
        GraphHiveSceneSurfaceWebglMap(const GraphHiveSceneSurfaceWebglMap& copyFrom);
        GraphHiveSceneSurfaceWebglMap& operator= (const GraphHiveSceneSurfaceWebglMap& copyFrom);

        /**
         * Serve the lightweight revision check browsers poll to detect a change to the bound surface.
         * @param response Response to populate.
         */
        void __serveRevision(HttpResponse& response);

        /**
         * Serve a poke request made by the rendered page when a chunk of the scene is clicked on or hovered over.
         * @param request The incoming poke request. Must carry both a "nodeId" and a "chunkId" query parameter,
         *        which together identify the poked chunk. May carry a "type" query parameter of "hoverEnter" or
         *        "hoverLeave" to raise a HOVER_ENTER or HOVER_LEAVE poke instead of the default HIT.
         * @param response Response to populate.
         */
        void __servePoke(HttpRequest& request, HttpResponse& response);

        virtual void hiveSurfaceChanged(GraphHandle<GraphHiveSurface> hiveSurface) override;

        virtual GraphHiveSurfaceListener* getListener() override;

        /// Scene surface this map is bound to for its whole lifetime.
        GraphHandle<GraphHiveSceneSurface> _sceneSurface;

        /// Bumped every time the bound surface reports a change, so polling browsers can detect it.
        unsigned _revision = 0;

        /// How often browsers viewing this map's page poll to check for changes, in milliseconds.
        unsigned _pollIntervalMs = 1000;

        /// Guards _revision, written from hiveSurfaceChanged() and read while rendering the page/serving requests.
        ThreadMutex _lock;
};

#endif
