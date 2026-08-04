#ifndef TRIGGER_ACTION_H
#define TRIGGER_ACTION_H

#include <string>

#include "../GraphNode.hpp"
#include "SerialisableAction.hpp"

class SerialisableActionPayload;

/**
 * Graph action that triggers nodes as it traverses the graph.
 */
class TriggerAction : public SerialisableAction
{
    public:

        virtual ~TriggerAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param nodeName If non-empty, restricts triggering to nodes with this name.
		 * @param restrictToNodeType If true, restricts triggering to nodes of nodeType.
		 * @param nodeType Node type required when restrictToNodeType is true.
		 */
		TriggerAction(Handle<GraphNode>& initNode, std::string nodeName = "", bool restrictToNodeType = false,
			GraphNodeType nodeType = GraphNodeType::GRAPH_NODE);

		SerialisableActionType getSerialisbleType() override;

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

		SerialisableActionPayload* _serialise() override;

		void _deserialise(SerialisableActionPayload& data) override;

    private:

        // Do not allow copying.
        TriggerAction(const TriggerAction& copyFrom);
        TriggerAction& operator= (const TriggerAction& copyFrom);

		/// If non-empty, restricts triggering to nodes with this name.
		std::string _nodeName;

		/// If true, restricts triggering to nodes of _nodeType.
		bool _restrictToNodeType;

		/// Node type required when _restrictToNodeType is true.
		GraphNodeType _nodeType;
};

#endif
