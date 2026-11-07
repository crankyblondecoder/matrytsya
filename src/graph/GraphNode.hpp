#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H

#include <atomic>
#include <string>
#include <vector>

class GraphAction;
class GraphEdge;
class GraphHive;
class SceneRootNode;
class StrobeEmitterNode;

#include "../util/RefCounted.hpp"
#include "GraphActionTargetable.hpp"
#include "../util/Handle.hpp"
#include "GraphNamed.hpp"
#include "GraphNodeType.hpp"
#include "GraphPoke.hpp"

// The number of edges a node can have is fixed.
#define EDGE_ARRAY_SIZE 32

// Edge traversal specificity weights. A key match outranks an action flag match, so the key weight must exceed
// the flag weight. An edge with both scores highest.
#define ACTION_FLAG_EDGE_SPECIFICITY 1
#define KEY_EDGE_SPECIFICITY 2
#define MAX_EDGE_SPECIFICITY (KEY_EDGE_SPECIFICITY + ACTION_FLAG_EDGE_SPECIFICITY)

/**
 * Node of a graph
 * @note Nodes self delete when no longer referenced by an edge.
 */
class GraphNode : public RefCounted, public GraphActionTargetable, public GraphNamed
{
    public:

		/**
		 * Create new graph node.
		 * @note Because this is ref-counted it will require the automatic initial reference increase to be released
		 *       before it can be deleted.
		 */
        GraphNode();

		/**
		 * Get the concrete type of this node.
		 * @note Default implementation reports GraphNodeType::GRAPH_NODE. Subclasses that need to be identified
		 *       as a specific kind of node override this to report their own type.
		 */
		virtual GraphNodeType getType();

		/**
		 * Get this node as a StrobeEmitterNode.
		 * @note Default implementation reports that the node is not one. StrobeEmitterNode overrides this, so
		 *       every subclass of it is reported as one too.
		 * @returns This node as a StrobeEmitterNode, or 0 if it is not one.
		 */
		virtual StrobeEmitterNode* asStrobeEmitter();

		/**
		 * Get this node as a SceneRootNode.
		 * @note Default implementation reports that the node is not one. SceneRootNode overrides this.
		 * @returns This node as a SceneRootNode, or 0 if it is not one.
		 */
		virtual SceneRootNode* asSceneRoot();

		/**
		 * Get the name a node type is exposed under.
		 * @param type Type to get the name of.
		 * @returns The type's name.
		 */
		static std::string typeName(GraphNodeType type);

		/**
		 * Get the node type a name stands for.
		 * @param name Name to look up.
		 * @returns The type the name stands for.
		 * @throw GraphException With error UNKNOWN_NODE_TYPE_NAME if the name is not recognised.
		 */
		static GraphNodeType typeFromName(const std::string& name);

		/**
		 * Get the name of every node type, in enum declaration order.
		 * @note A type absent from this list cannot be round-tripped through typeName()/typeFromName(), so
		 *       this must be extended whenever GraphNodeType is.
		 */
		static std::vector<std::string> typeNames();

		/**
		 * Create and add an edge from this node to another node.
		 * ie The edge is directed from this node to another node.
		 * @note Only nodes can create edges.
		 * @param connectTo Handle of node to form edge to.
		 * @param actionFlags Action flags to add to edge. These will determine which actions can traverse this edge.
		 *        Leave blank for no restriction.
		 * @param traversalKey If non-zero, only an action holding this exact key may traverse the edge. Zero for no
		 *        key restriction. Fixed for the life of the edge.
         * @returns Handle of created edge. Will be invalid if edge could not be created.
		 */
		Handle<GraphEdge> createEdge(Handle<GraphNode>& connectTo, std::vector<unsigned long> actionFlags,
			unsigned traversalKey = 0);

		/**
		 * Remove edge from this node.
		 * @param handle Handle of edge to remove.
		 */
        void removeEdge(Handle<GraphEdge> handle);

		/**
		 * Find the next node to traverse to.
		 * @note This defines default traversal behaviour which is to traverse the next edge that the action has not
		 *       already traversed.
		 * @note The most specifically matched traversable edge is taken: a key match outranks an action flag match,
		 *       which outranks an unflagged wildcard edge, and an edge matching both key and flags ranks highest.
		 *       Ties go to the earliest edge in the edge array.
		 * @param action Action that is requesting to traverse.
		 * @returns Handle to next edge to traverse.
		 */
		virtual Handle<GraphEdge> traverse(GraphAction& action);

		/**
		 * Get a snapshot of all edges directed from this node.
		 * @returns A handle to each edge currently on this node.
		 */
		std::vector<Handle<GraphEdge>> getEdges();

		/**
		 * Get the energy cost of an action being applied to this node.
		 */
		unsigned getEnergyCost();

		/**
		 * Does any house keeping associated with decoupling from the graph.
		 * Once a node is decoupled, it can't be re-attached.
		 * @note This can be assumed to be associated with being removed from a hive.
		 */
		void decouple();

		/**
		 * Get a handle to the hive this node is part of.
		 */
		Handle<GraphHive> getHive();

		/**
		 * Get the unique id of this node.
		 */
		unsigned getId();

		/**
		 * Set the hive that this node belongs to.
		 * @param hive Hive that this node is part of.
		 * @returns True if this node accepts being part of the hive. False otherwise.
		 */
		bool setHive(Handle<GraphHive> hive);

		/// When true, every poke accepted by a node is reported on standard output.
		static bool _logToConsole;

		/**
		 * Poke this node.
		 */
		virtual void poke(GraphPoke poke) final;

		/**
		 * Get whether poking is enabled.
		 */
		bool getPokeEnabled();

		/**
		 * Set whether poking is enabled.
		 */
		void setPokeEnabled(bool enable);

		/**
		 * Get whether this node still accepts actions being applied to it.
		 * @returns False once the node has been decoupled from the graph.
		 */
		bool getActionable();

    protected:

		// Must be virtual for reference counting auto-delete.
		virtual ~GraphNode();

		/**
		 * Set the energy cost of an action being applied to this node.
		 */
		void _setEnergyCost(unsigned cost);

		/**
		 * Emit an action by making its origin this node.
		 * @note Default implementation simply starts the action. Subclasses that manage the actions they emit,
		 *       eg by serialising them, override this to route the action through that mechanism.
		 * @param action Action to emit. This must have its refcount increased prior to the call.
		 */
		virtual void _emitAction(GraphAction* action);

		/**
		 * Subclass hook to notify that this node has been poked.
		 */
		virtual void _poked(GraphPoke poke) = 0;

	private:

        /// Generic lock.
        ThreadMutex _lock;

		/// Counter used to derive each node's unique id.
		static std::atomic<unsigned> _nextId;

		/// Unique id of this node.
		unsigned _id;

		/// The hive this node belongs to.
		Handle<GraphHive> _hive;

		/// All edges directed from this node.
        GraphEdge* _edges[EDGE_ARRAY_SIZE]{};

		/// Whether edge slot in _edges array has been allocated.
		bool _edgeAlloc[EDGE_ARRAY_SIZE]{};

		/// Count of the number of edges in the edge array.
		unsigned _edgeCount = 0;

		/// Whether this node is in the process of decoupling or has been decoupled from all edges it contains.
		std::atomic<bool> _decoupled{false};

		/// How much energy it costs for an action to be applied to this node.
		unsigned _actionEnergyCost = 1;

		/// Whether poking is enabled for this node. If false, all pokes are immediately discarded.
		bool _pokeEnabled = false;

		// Emits a TriggerAction against an arbitrary bound node on an AI model's behalf, so it needs access to
		// _emitAction the same way a GraphNode subclass would.
		friend class TriggerEmitterToolBindings;

        // Do not allow copying.
        GraphNode(const GraphNode& copyFrom);
        GraphNode& operator= (const GraphNode& copyFrom);

		/**
		 * Remove edge from this node.
		 * @note This method needs to be externally locked.
		 * @param edgeIndex Index into edges array of edge to remove.
		 * @returns Point to graph edge that needs to be deleted outside of lock.
		 */
        GraphEdge* __removeEdge(int edgeIndex);

		/**
		 * Score how specifically a traversable edge matches the action traversing it.
		 * @param edge An edge the action has already been permitted to traverse.
		 * @returns 0 for a wildcard edge, up to MAX_EDGE_SPECIFICITY for an edge that is both keyed and flagged.
		 * @note Only valid for an edge that has passed both traversal checks, since a keyed edge only passes
		 *       when the action holds its key, and a flagged edge only when the action matches a flag.
		 */
		int __getEdgeSpecificity(GraphEdge* edge);
};

#endif
