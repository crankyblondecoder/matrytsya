#ifndef GRAPH_NODE_VISITOR_H
#define GRAPH_NODE_VISITOR_H

class AgentNode;
class BuilderScriptNode;
class GraphNode;
class PingNode;
class SceneGeometryNode;
class SceneGeometryScriptNode;
class SceneImageNode;
class SceneRootNode;
class SceneTextScriptNode;
class SceneTransformNode;
class SceneTransformScriptNode;
class TeleportNode;
class TriggerNode;

/**
 * Visitor over the concrete node types a hive can be built from, for code outside the graph module that needs a
 * node's concrete type without discovering it through RTTI.
 * @note Dispatch is double: GraphNode::accept() is a virtual call, and each concrete node's override calls the
 *       visit() overload matching its own static type. A node of a type with no overload of its own lands in
 *       visit(GraphNode&).
 */
class GraphNodeVisitor
{
	public:

		virtual ~GraphNodeVisitor() {}

		/**
		 * Visit a node whose concrete type has no visit() overload of its own.
		 * @param node Node being visited.
		 */
		virtual void visit(GraphNode& node) = 0;

		virtual void visit(PingNode& node) = 0;
		virtual void visit(TeleportNode& node) = 0;
		virtual void visit(SceneRootNode& node) = 0;
		virtual void visit(SceneGeometryNode& node) = 0;
		virtual void visit(SceneGeometryScriptNode& node) = 0;
		virtual void visit(SceneTransformNode& node) = 0;
		virtual void visit(SceneTransformScriptNode& node) = 0;
		virtual void visit(SceneTextScriptNode& node) = 0;
		virtual void visit(AgentNode& node) = 0;
		virtual void visit(TriggerNode& node) = 0;
		virtual void visit(BuilderScriptNode& node) = 0;
		virtual void visit(SceneImageNode& node) = 0;
};

#endif
