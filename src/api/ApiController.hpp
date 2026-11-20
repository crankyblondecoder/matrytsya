#ifndef API_CONTROLLER_H
#define API_CONTROLLER_H

#include <map>
#include <string>
#include <vector>

#include "../util/Handle.hpp"

class GraphHive;
class GraphHiveCollection;
class GraphNode;
class HiveHost;
class HiveRecorder;
class HttpRequest;
class HttpResponse;

/**
 * Carries out the requests of the hive control API.
 *
 * Each route is a method and a path pattern below the API's base path, e.g. "GET /hives/{hive}". Every response
 * body is JSON. A request that cannot be carried out raises an exception, which the caller turns into an error
 * response.
 *
 * @note The routes listed by getRoutes() must be kept in step with doc/hiveApi.openapi.json, which documents them.
 *       A unit test checks that the two agree.
 * @note Requests are expected to be dispatched one at a time, as the HTTP server does. The hive itself guards
 *       against a request racing anything going on within it, such as a builder script adding nodes.
 */
class ApiController
{
	public:

		/**
		 * One route of the API.
		 */
		struct RouteDescription
		{
			/// HTTP method, e.g. "GET".
			std::string method;

			/// Path pattern below the API's base path, with each parameter in braces, e.g. "/hives/{hive}".
			std::string pattern;
		};

		/**
		 * @param host Host of every hive the API controls.
		 * @param collection Collection the hosted hives are in.
		 */
		ApiController(HiveHost& host, GraphHiveCollection& collection);

		virtual ~ApiController();

		/**
		 * Carry out a request.
		 * @param request Request to carry out.
		 * @param path Path of the request below the API's base path, e.g. "/hives/garden". Still URL encoded.
		 * @param response Response to populate.
		 * @throw ApiException If the request cannot be carried out.
		 * @throw PersistException If a hive or node definition in the request is invalid, or a hive could not be
		 *        stored.
		 * @throw GraphException If the hive could not carry out the request.
		 */
		void dispatch(HttpRequest& request, const std::string& path, HttpResponse& response);

		/**
		 * Get every route of the API.
		 */
		static std::vector<RouteDescription> getRoutes();

	private:

		// Disable copying.
		ApiController(const ApiController& copyFrom);
		ApiController& operator= (const ApiController& copyFrom);

		/// Path parameters of a request, by the name given in its route's pattern, URL decoded.
		typedef std::map<std::string, std::string> Params;

		/// Carries out the requests of one route.
		typedef void (ApiController::*Handler)(Params& params, HttpRequest& request, HttpResponse& response);

		/**
		 * One route of the API, with what carries it out.
		 */
		struct Route
		{
			const char* method;
			const char* pattern;
			Handler handler;
		};

		/**
		 * Get every route of the API, with what carries each out.
		 */
		static const std::vector<Route>& __routes();

		// -- Route handlers --

		void __getIndex(Params& params, HttpRequest& request, HttpResponse& response);
		void __getOpenApi(Params& params, HttpRequest& request, HttpResponse& response);
		void __getHiveSchema(Params& params, HttpRequest& request, HttpResponse& response);
		void __listHives(Params& params, HttpRequest& request, HttpResponse& response);
		void __createHive(Params& params, HttpRequest& request, HttpResponse& response);
		void __getHive(Params& params, HttpRequest& request, HttpResponse& response);
		void __deleteHive(Params& params, HttpRequest& request, HttpResponse& response);
		void __getHiveDefinition(Params& params, HttpRequest& request, HttpResponse& response);
		void __listNodes(Params& params, HttpRequest& request, HttpResponse& response);
		void __createNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __getNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __updateNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __deleteNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __createEdge(Params& params, HttpRequest& request, HttpResponse& response);
		void __deleteEdge(Params& params, HttpRequest& request, HttpResponse& response);
		void __pokeNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __triggerNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __animateNode(Params& params, HttpRequest& request, HttpResponse& response);
		void __setStrobeEmitter(Params& params, HttpRequest& request, HttpResponse& response);
		void __clearStrobeEmitter(Params& params, HttpRequest& request, HttpResponse& response);
		void __setStrobeSurface(Params& params, HttpRequest& request, HttpResponse& response);
		void __clearStrobeSurface(Params& params, HttpRequest& request, HttpResponse& response);

		// -- Helpers --

		/**
		 * Find a hosted hive.
		 * @throw ApiException(NOT_FOUND) If no hive of that name is hosted.
		 */
		Handle<GraphHive> __findHive(const std::string& hiveName);

		/**
		 * Find the recorder of a hosted hive.
		 * @throw ApiException(NOT_FOUND) If no hive of that name is hosted.
		 */
		Handle<HiveRecorder> __findRecorder(const std::string& hiveName);

		/**
		 * Find a node of a hive.
		 * @throw ApiException(NOT_FOUND) If the hive holds no node of that name.
		 */
		Handle<GraphNode> __findNode(Handle<GraphHive>& hive, const std::string& nodeName);

		/**
		 * Refuse a change to a node that one of its hive's surfaces is bound to.
		 * @throw ApiException(CONFLICT) If a surface is bound to it.
		 */
		void __checkNotBoundToSurface(Handle<HiveRecorder>& recorder, const std::string& nodeName);

		/**
		 * Write a hive's summary as JSON.
		 */
		std::string __hiveSummary(Handle<GraphHive>& hive);

		/**
		 * Write a node's full view as JSON: its definition and its edges.
		 */
		std::string __nodeView(Handle<HiveRecorder>& recorder, Handle<GraphNode>& node);

		/// Host of every hive the API controls.
		HiveHost& _host;

		/// Collection the hosted hives are in.
		GraphHiveCollection& _collection;
};

#endif
