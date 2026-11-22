#ifndef HIVE_NODE_CAPTURE_H
#define HIVE_NODE_CAPTURE_H

#include <vector>

#include "HiveNodeDescriptor.hpp"
#include "../graph/GraphNodeVisitor.hpp"

class SceneGeometry;
class ScriptNode;

/**
 * Reads a live node back into the HiveNodeDescriptor it could be rebuilt from, as the inverse of
 * HiveBuilder's node creation.
 * @note Captures whatever the node holds when it is visited, so capturing a node as it joins a hive gives the
 *       values it was created with rather than whatever its scripts have since changed them to.
 */
class HiveNodeCapture : public GraphNodeVisitor
{
	public:

		HiveNodeCapture();

		virtual ~HiveNodeCapture();

		/**
		 * Capture a node.
		 * @param node Node to capture.
		 * @returns Descriptor the node could be rebuilt from, edges included.
		 * @throw PersistException(HIVE_NOT_CAPTURABLE) If the node is of a type no hive definition can describe.
		 * @throw PersistException(UNKNOWN_ACTION_FLAG) If one of its edges carries a flag a hive definition
		 *        cannot name.
		 */
		static HiveNodeDescriptor capture(GraphNode& node);

		/**
		 * Capture the edges currently directed from a node.
		 * @param node Node whose edges are captured.
		 * @returns One descriptor per edge whose target node still exists, in edge slot order.
		 * @throw PersistException(UNKNOWN_ACTION_FLAG) If an edge carries a flag a hive definition cannot name.
		 */
		static std::vector<HiveEdgeDescriptor> captureEdges(GraphNode& node);

		virtual void visit(GraphNode& node) override;
		virtual void visit(PingNode& node) override;
		virtual void visit(TeleportNode& node) override;
		virtual void visit(SceneRootNode& node) override;
		virtual void visit(SceneGeometryNode& node) override;
		virtual void visit(SceneGeometryScriptNode& node) override;
		virtual void visit(SceneTransformNode& node) override;
		virtual void visit(SceneTransformScriptNode& node) override;
		virtual void visit(SceneTextScriptNode& node) override;
		virtual void visit(AgentNode& node) override;
		virtual void visit(TriggerNode& node) override;
		virtual void visit(BuilderScriptNode& node) override;
		virtual void visit(SceneImageNode& node) override;

	private:

		// Disable copying.
		HiveNodeCapture(const HiveNodeCapture& copyFrom);
		HiveNodeCapture& operator= (const HiveNodeCapture& copyFrom);

		/**
		 * Capture the vertex groups of a geometry node.
		 * @param geometry Geometry to capture.
		 */
		void __captureGeometry(const SceneGeometry& geometry);

		/**
		 * Capture the core and poke scripts of a script node.
		 * @param node Script node to capture.
		 */
		void __captureScripts(ScriptNode& node);

		/// Descriptor being filled in by whichever visit() the node dispatched to.
		HiveNodeDescriptor _descriptor;
};

#endif
