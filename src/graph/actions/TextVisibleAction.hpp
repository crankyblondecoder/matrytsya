#ifndef TEXT_VISIBLE_ACTION_H
#define TEXT_VISIBLE_ACTION_H

#include "../GraphAction.hpp"

/**
 * Graph action that shows or hides the text of nodes as it traverses the graph.
 */
class TextVisibleAction : public GraphAction
{
    public:

        virtual ~TextVisibleAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param visible Text visibility this action applies to each visited node.
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them.
		 */
		TextVisibleAction(Handle<GraphNode> initNode, bool visible, std::vector<unsigned> edgeTraversalKeys = {});

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        TextVisibleAction(const TextVisibleAction& copyFrom);
        TextVisibleAction& operator= (const TextVisibleAction& copyFrom);

		/// Text visibility this action applies to each visited node.
		bool _visible;
};

#endif
