#ifndef TRIGGER_ACTION_H
#define TRIGGER_ACTION_H

#include <string>

#include "../GraphAction.hpp"
#include "../GraphNode.hpp"

/**
 * Graph action that triggers nodes as it traverses the graph.
 */
class TriggerAction : public GraphAction
{
    public:

        virtual ~TriggerAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param nodeName If non-empty, restricts triggering to nodes with this name.
		 * @param restrictToNodeType If true, restricts triggering to nodes of nodeType.
		 * @param nodeType Node type required when restrictToNodeType is true.
		 */
		TriggerAction(Handle<GraphNode> initNode, std::string nodeName = "", bool restrictToNodeType = false,
			GraphNode::Type nodeType = GraphNode::Type::GRAPH_NODE);

	protected:

		void _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        TriggerAction(const TriggerAction& copyFrom);
        TriggerAction& operator= (const TriggerAction& copyFrom);

		/// If non-empty, restricts triggering to nodes with this name.
		std::string _nodeName;

		/// If true, restricts triggering to nodes of _nodeType.
		bool _restrictToNodeType;

		/// Node type required when _restrictToNodeType is true.
		GraphNode::Type _nodeType;
};

#endif
