#ifndef GRAPH_HIVE_GRAPH_VIEW_SURFACE_WEBGL_MAP_H
#define GRAPH_HIVE_GRAPH_VIEW_SURFACE_WEBGL_MAP_H

#include <string>

#include "../util/Handle.hpp"
#include "GraphHiveSurfaceHtmlMap.hpp"

class GraphHiveGraphViewSurface;
class HttpServerBase;
class HttpRequest;
class HttpResponse;

/**
 * Draws a GraphHiveGraphViewSurface as a 3D WebGL scene in a browser, showing the hive's nodes and the edges
 * between them.
 * The catalogue the surface reports carries no positions, so the page it serves lays the graph out itself and
 * this map only ever sends structure: what the nodes are, and which node each edge leads to along with the
 * actions it supports. Every piece of text on the page, i.e. the name and type of a node and the actions of an
 * edge, is HTML drawn over the canvas rather than glyphs built into the scene.
 * @note Read only, as the bound surface is. The page never pokes, so nothing it shows can reach back into the
 *       graph it is drawn from.
 */
class GraphHiveGraphViewSurfaceWebglMap : public GraphHiveSurfaceHtmlMap
{
    public:

        /**
         * @param httpServer Server to register this map with. Not owned by this.
         * @param surface Graph view surface this map binds to for its whole lifetime. This map keeps its own
         *        reference to it (see Handle), released once this map is destroyed.
         * @param path Path to mount this map's interface at on httpServer, e.g. "/graph".
         */
        GraphHiveGraphViewSurfaceWebglMap(HttpServerBase& httpServer, GraphHiveGraphViewSurface& surface,
            std::string path);

    protected:

        // Required by ref counting.
        virtual ~GraphHiveGraphViewSurfaceWebglMap();

        virtual void _renderPage(HttpResponse& response) override;

        /**
         * Serve the graph the page draws, as
         * {"version":N,"nodes":[{"id":N,"name":"...","type":"...","edges":[{"toNodeId":N,"actions":["..."]}]}]}.
         * The version is the hive version the graph was catalogued at, and an edge's actions are the names of
         * the flags it carries, which are the same names a hive file refers to them by.
         * @note Sent whole every time rather than as a difference against what the viewer already has. It is
         *       ids, short names and flags, and the revision endpoint already keeps a page from asking for it
         *       at all while nothing has changed.
         * @param request The incoming data request.
         * @param response Response to populate with the graph.
         */
        virtual void _serveMapData(HttpRequest& request, HttpResponse& response) override;

    private:

        // Disable copying.
        GraphHiveGraphViewSurfaceWebglMap(const GraphHiveGraphViewSurfaceWebglMap& copyFrom);
        GraphHiveGraphViewSurfaceWebglMap& operator= (const GraphHiveGraphViewSurfaceWebglMap& copyFrom);

        /// Graph view surface this map is bound to for its whole lifetime. Held as the surface it is, rather
        /// than reached through the base class, so that the graph to draw can be asked of it.
        Handle<GraphHiveGraphViewSurface> _graphViewSurface;
};

#endif
