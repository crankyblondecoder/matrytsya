#ifndef GRAPH_HIVE_H
#define GRAPH_HIVE_H

#include <string>
#include <vector>

#include "../agent/AgenticHarness.hpp"
#include "../thread/ThreadCondition.hpp"
#include "../thread/ThreadMutex.hpp"
#include "../thread/ThreadPool.hpp"
#include "../util/RefCounted.hpp"
#include "./actions/SerialisableActionPayload.hpp"
#include "../util/Handle.hpp"
#include "GraphHiveCollection.hpp"
#include "GraphNamed.hpp"
#include "GraphNodeLocation.hpp"
#include "GraphPoke.hpp"
#include "GraphToolBindingsFactory.hpp"
#include "nodes/StrobeEmitterNode.hpp"

class GraphNode;
class GraphHiveStrobeScheduler;
class GraphHiveSceneSurface;
class GraphHiveSurface;
class ModelContext;

/**
 * A "Hive" is a container for nodes.
 * Nodes can refer to the hive when they want access to specific services, like persistence for example.
 */
class GraphHive : public RefCounted, public GraphNamed
{
    public:

		/**
		 * Constructor
		 * @note Will wait on its internal thread pool to become active.
		 * @param numThreads The number of threads to create for the thread pool that the hive does processing on.
		 */
		GraphHive(unsigned numThreads);

		/**
		 * Shutdown this hive.
		 * This will dismantle the hive's graph.
		 */
		void shutdown();

		/**
		 * Add a graph node to this hive.
		 * @note Expects to manage the initial reference count of this node regardless of whether it could be added.
		 */
		void addNode(GraphNode* node);

		/**
		 * Remove node from hive.
		 */
		void removeNode(Handle<GraphNode> nodeHandle);

		/**
		 * Find a node in this hive by name.
		 * @param nodeName Name of node to find.
		 * @returns Handle to the node. Invalid handle if no node with that name exists in this hive.
		 */
		Handle<GraphNode> getNode(std::string nodeName);

		/**
		 * Get the names of all the nodes in this hive.
		 */
		std::vector<std::string> getNodeNames();

		/**
		 * Poke this hive.
		 * @note If a poke can't be affected, it is simply discarded.
		 * @param nodeId The id of the node that is to be poked within the hive.
		 * @param poke Poke to apply.
		 */
		void poke(unsigned nodeId, GraphPoke poke);

		/**
		 * Add a graph hive surface to this hive.
		 * @note Expects to manage the initial reference count of this surface regardless of whether it could be added.
		 */
		void addSurface(GraphHiveSurface* surface);

		/**
		 * Remove surface from hive.
		 */
		void removeSurface(Handle<GraphHiveSurface> surfaceHandle);

		/**
		 * Find a surface in this hive by name.
		 * @param surfaceName Name of surface to find.
		 * @returns Handle to the surface. Invalid handle if no surface with that name exists in this hive.
		 */
		Handle<GraphHiveSurface> getSurface(std::string surfaceName);

		/**
		 * Find a scene surface in this hive by name.
		 * @param surfaceName Name of surface to find.
		 * @returns Handle to the surface. Invalid handle if no surface with that name exists in this
		 *          hive, or it exists but is not a GraphHiveSceneSurface.
		 */
		Handle<GraphHiveSceneSurface> getSceneSurface(std::string surfaceName);

		/**
		 * Get the default scene surface in this hive.
		 * @returns Handle to the first surface, in this hive's surfaces, that is both a
		 *          GraphHiveSceneSurface and marked as default. Invalid handle if no such surface exists.
		 */
		Handle<GraphHiveSceneSurface> getDefaultSceneSurface();

		/**
		 * Register a node as a periodic strobe emitter within this hive, or update an existing
		 * registration's frequency.
		 * @note Silently ignored if the node is not a StrobeEmitterNode, the handle is invalid or
		 *       periodMs is 0. The node stops being an emitter automatically when it is removed
		 *       or decoupled from the hive.
		 * @param nodeHandle Handle of the node to register.
		 * @param periodMs Emission period in milliseconds (time between successive emissions).
		 */
		void setStrobeEmitter(Handle<StrobeEmitterNode> nodeHandle, unsigned periodMs);

		/**
		 * Remove a node as a periodic strobe emitter within this hive.
		 * @note Safe to call for a node that is not currently a strobe emitter (no-op).
		 * @param nodeHandle Handle of the node to remove.
		 */
		void clearStrobeEmitter(Handle<StrobeEmitterNode> nodeHandle);

		/**
		 * Register a surface as being periodically strobed within this hive, or update an existing
		 * registration's frequency.
		 * @note Silently ignored if the handle is invalid or periodMs is 0. The surface stops being
		 *       strobed automatically when it is removed from the hive.
		 * @param surfaceHandle Handle of the surface to register.
		 * @param periodMs Strobe period in milliseconds (time between successive strobes).
		 */
		void setStrobeSurface(Handle<GraphHiveSurface> surfaceHandle, unsigned periodMs);

		/**
		 * Remove a surface from being periodically strobed within this hive.
		 * @note Safe to call for a surface that is not currently being strobed (no-op).
		 * @param surfaceHandle Handle of the surface to remove.
		 */
		void clearStrobeSurface(Handle<GraphHiveSurface> surfaceHandle);

		/**
		 * Get the thread pool used by this hive to enumerate itself.
		 * @param numTabs Number of tabs to indent output by.
		 */
		void enumerateThreadPool(unsigned numTabs);

		/**
		 * Execute a work unit using this hives thread pool.
		 * @note The work unit will be automatically deleted once it has completed.
		 * @param workUnit Work unit to execute.
		 * @returns True if could execute. False otherwise.
		 */
		bool executeWorkUnit(ThreadPoolWorkUnit* workUnit);

		/**
		 * Set the graph hive collection this hive is part of.
		 */
		void setHiveCollection(GraphHiveCollection* collection);

		/**
		 * Set the agentic harness this hive can use to drive agentic decisions.
		 * @param agenticHarness Handle of the harness to set. May be an invalid handle to clear.
		 */
		void setAgenticHarness(Handle<AgenticHarness> agenticHarness);

		/**
		 * Get the agentic harness set on this hive.
		 * @returns Handle to the agentic harness. Invalid handle if none has been set.
		 */
		Handle<AgenticHarness> getAgenticHarness();

		/**
		 * Set the tool bindings factory this hive supplies to whatever within it needs the tool bindings of a
		 * concrete class.
		 * @param toolBindingsFactory Handle of the factory to set. May be an invalid handle to clear.
		 * @note The concrete factory lives in the agent_bindings module, which depends on this one, so the
		 *       factory is built outside the graph and set here rather than being built by this hive.
		 */
		void setToolBindingsFactory(Handle<GraphToolBindingsFactory> toolBindingsFactory);

		/**
		 * Get the tool bindings factory set on this hive.
		 * @returns Handle to the tool bindings factory. Invalid handle if none has been set.
		 */
		Handle<GraphToolBindingsFactory> getToolBindingsFactory();

		/**
		 * Create a new model context for an agentic conversation, without processing any prompt against it.
		 * @note Delegates to the agentic harness.
		 * @param role Role the context is being created for.
		 * @param capability Capability required of the model that will eventually service requests made in
		 *        the context.
		 * @returns The new context, built from the system prompts and tools assigned to the given role and
		 *          capability.
		 * @throw GraphException When this hive has no agentic harness set.
		 */
		Handle<ModelContext> createModelContext(AgenticHarness::Role role, AgenticHarness::Capability capability);

		/**
		 * Process an agentic request against this hive's agentic harness.
		 * @note Delegates to the agentic harness with the role fixed to AgenticHarness::Role::CHAT.
		 * @param capability Capability required of the model.
		 * @param prompt Text of the prompt to send to the model.
		 * @param context Context of a previous interaction to continue. When not supplied, a new
		 *        context is built from the system prompts and tools assigned to the role and
		 *        capability.
		 * @returns The context the request was serviced within.
		 * @throw GraphException When this hive has no agentic harness set.
		 * @throw AgentException When no prompt text was supplied, or when no candidate model is
		 *        assigned to the role with that capability.
		 */
		Handle<ModelContext> processAgenticRequest(AgenticHarness::Capability capability, std::string prompt,
			Handle<ModelContext> context = Handle<ModelContext>(0));

		/**
		 * Process an agentic request against this hive's agentic harness, on behalf of a specific node.
		 * @note Delegates to the agentic harness with the role fixed to AgenticHarness::Role::NODE.
		 * @param capability Capability required of the model.
		 * @param prompt Text of the prompt to send to the model.
		 * @param context Context of a previous interaction to continue. When not supplied, a new
		 *        context is built from the system prompts and tools assigned to the role and
		 *        capability.
		 * @returns The context the request was serviced within.
		 * @throw GraphException When this hive has no agentic harness set.
		 * @throw AgentException When no prompt text was supplied, or when no candidate model is
		 *        assigned to the role with that capability.
		 */
		Handle<ModelContext> processNodeAgenticRequest(AgenticHarness::Capability capability, std::string prompt,
			Handle<ModelContext> context = Handle<ModelContext>(0));

		/**
		 * Teleport a graph action.
		 * @param actionPayload Payload of action to teleport.
		 * @param nodeLocation Location of node to teleport action to.
		 */
		void teleportAction(SerialisableActionPayload& actionPayload, GraphNodeLocation& nodeLocation);

		/**
		 * Notify this hive that a graph action has become active.
		 * This occurs when the action starts.
		 * @returns Handle to use when notifying this hive that the action has become inactive.
		 */
		unsigned actionActive(GraphAction* action);

		/**
		 * Notify this have that a graph action has become inactive.
		 * This occurs when the action is complete.
		 * @param handle Handle that was supplied by actionActive.
		 */
		void actionInactive(unsigned handle);

		/**
		 * Wait on there being no active actions within this hive.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnNoActionsActive(unsigned timeOut);

		/**
		 * Wait on the initial (first) action becoming active within this hive.
		 * This will only wait on the very first action becoming active in this hive.
		 * @note This is intended to only be used for unit testing purposes.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnInitialActionActive(unsigned timeOut);

		/**
		 * Wait on the accumulated active action count going greater of equal to a particular value.
		 * @param count Value that count accumulator must be greater or equal to.
		 * @param timeOut Maximum period in ms to wait on condition to be signalled. Use 0 for no timeout.
		 */
		void waitOnActionActiveCountAccum(int count, unsigned timeOut);

	protected:

		// Required by ref counting.
        virtual ~GraphHive();

    private:

		/**
		 * Find a node in this hive by node id.
		 * @param nodeId Id of node to find.
		 * @returns Node handle. If no node was found this will be invalid.
		 */
		Handle<GraphNode> __findNode(unsigned nodeId);

		/**
		 * Process an agentic request against this hive's agentic harness for the given role.
		 * @param role Role to process the request as.
		 * @param capability Capability required of the model.
		 * @param prompt Text of the prompt to send to the model.
		 * @param context Context of a previous interaction to continue.
		 * @returns The context the request was serviced within.
		 * @throw GraphException When this hive has no agentic harness set.
		 */
		Handle<ModelContext> __processAgenticRequest(AgenticHarness::Role role, AgenticHarness::Capability capability,
			std::string prompt, Handle<ModelContext> context);

		/// Thread pool that hive runs actions on.
		ThreadPool* _threadPool = 0;

		/// Dedicated thread that drives per-node strobe emission for this hive.
		GraphHiveStrobeScheduler* _strobeScheduler = 0;

		/// Hive collection this hive is part of.
		GraphHiveCollection* _collection = 0;

		/// Agentic harness this hive can use to drive agentic decisions.
		Handle<AgenticHarness> _agenticHarness;

		/// Factory this hive supplies the tool bindings of a concrete class from.
		Handle<GraphToolBindingsFactory> _toolBindingsFactory;

		/// Nodes contained in this hive.
		std::vector<GraphNode*>	_nodes;

		/// Surfaces contained in this hive.
		std::vector<GraphHiveSurface*> _surfaces;

		/// Currently active actions within the hive.
		std::vector<GraphAction*> _activeActions;

		/// Whether this hive is active.
		bool _active = false;

        /// Generic lock.
        ThreadMutex _lock;

		/**
		 * Number of currently active actions within the hive.
		 * A value of -1 indicates shutdown is in progress.
		 */
		int _activeActionCount = 0;

		/**
		 * The accumulated number of actions that have become active.
		 * A value of -1 indicates shutdown is in progress;
		 */
		int _activeActionCountAccum = 0;

		/// This flag indiciates that an initial action became active. This will only ever flip to true once.
		bool _initialActionActive = false;

		/// Condition that guards the current active action count.
		ThreadCondition _activeActionCountCond;

		/// Condition that guards the active action count accumulator.
		ThreadCondition _activeActionCountAccumCond;
};

#endif
