#ifndef HIVE_BUILDER_H
#define HIVE_BUILDER_H

#include <string>

// All three are needed in full because the node type, capability and vertex visibility translation
// helpers return their enums.
#include "../agent/AgenticHarness.hpp"
#include "../graph/GraphNode.hpp"
#include "../graph/nodes/SceneGeometry.hpp"
#include "../util/Handle.hpp"

class GraphHive;
class GraphHiveSurface;
class HiveLoader;
struct HiveNodeDescriptor;
struct HiveSurfaceDescriptor;

/**
 * Builds a fully populated GraphHive from any HiveLoader.
 * @note Format-agnostic: all hive data (name, nodes, edges, surfaces, strobe emitters, strobe
 *       surfaces) comes from the loader, so a new persisted format only needs a new HiveLoader
 *       subclass, never a change here.
 */
class HiveBuilder
{
	public:

		/**
		 * Build a fully populated hive from a loader.
		 * @param loader Loader supplying the hive's name, nodes, edges, surfaces and strobe
		 *        emitter/surface registrations.
		 * @param numThreads Number of threads to give the constructed hive's thread pool.
		 * @returns Newly allocated, fully wired GraphHive. Caller takes ownership of the initial reference.
		 * @throw PersistException On any structural problem in the loader's data.
		 */
		static GraphHive* build(HiveLoader& loader, unsigned numThreads);

	private:

		// Not instantiable.
		HiveBuilder();
		HiveBuilder(const HiveBuilder& copyFrom);
		HiveBuilder& operator= (const HiveBuilder& copyFrom);

		/**
		 * Create the concrete GraphNode subclass described by a descriptor, populated with all of its
		 * type-specific data.
		 * @param descriptor Descriptor of the node to create.
		 * @returns Newly allocated node.
		 * @throw PersistException(UNKNOWN_NODE_TYPE) If the descriptor's type is not recognised.
		 */
		static GraphNode* __createNode(const HiveNodeDescriptor& descriptor);

		/**
		 * Create the concrete GraphHiveSurface subclass described by a descriptor.
		 * @param descriptor Descriptor of the surface to create.
		 * @param referencedNode Node this surface binds to, already resolved by name (invalid handle
		 *        if the descriptor's type does not reference a node).
		 * @param hasInitialFocusNode Whether descriptor.initialFocusNodeName was set and resolved.
		 * @param initialFocusNodeId Id of the resolved initial-focus node. Only meaningful if
		 *        hasInitialFocusNode is true.
		 * @returns Newly allocated surface.
		 * @throw PersistException(SURFACE_NODE_WRONG_TYPE) If referencedNode exists but is the wrong
		 *        concrete type for this surface type.
		 * @throw PersistException(UNKNOWN_SURFACE_TYPE) If the descriptor's type is not recognised.
		 */
		static GraphHiveSurface* __createSurface(const HiveSurfaceDescriptor& descriptor, Handle<GraphNode> referencedNode,
			bool hasInitialFocusNode, unsigned initialFocusNodeId);

		/**
		 * Translate an action flag name into its bit value.
		 * @param name Action flag name, as it appears in graphActionFlagRegister.hpp.
		 * @throw PersistException(UNKNOWN_ACTION_FLAG) If the name is not recognised.
		 */
		static unsigned long __actionFlagFromName(const std::string& name);

		/**
		 * Translate a capability name into its enum value.
		 * @param name Capability name, as it appears in AgenticHarness::Capability.
		 * @throw PersistException(UNKNOWN_AGENT_CAPABILITY) If the name is not recognised.
		 */
		static AgenticHarness::Capability __capabilityFromName(const std::string& name);

		/**
		 * Translate a node type name into its enum value.
		 * @param name Node type name, as it appears in GraphNodeType.
		 * @throw PersistException(UNKNOWN_AGENT_PROMPT_NODE_TYPE) If the name is not recognised.
		 */
		static GraphNodeType __nodeTypeFromName(const std::string& name);

		/**
		 * Translate a vertex visibility name into its enum value.
		 * @param name Vertex visibility name, as it appears in SceneGeometry::VertexVisibility.
		 * @throw PersistException(UNKNOWN_VERTEX_VISIBILITY) If the name is not recognised.
		 */
		static SceneGeometry::VertexVisibility __vertexVisibilityFromName(const std::string& name);

		/**
		 * Append a descriptor's vertexes to the geometry of the node built from it, one group at a time so
		 * that each keeps the visibility it was loaded with.
		 * @param geometry Geometry of the node being built.
		 * @param descriptor Descriptor the node is being built from.
		 * @throw PersistException(UNKNOWN_VERTEX_VISIBILITY) If a group names a visibility that does not exist.
		 */
		static void __addVertexGroups(SceneGeometry* geometry, const HiveNodeDescriptor& descriptor);
};

#endif
