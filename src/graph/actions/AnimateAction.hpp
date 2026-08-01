#ifndef ANIMATE_ACTION_H
#define ANIMATE_ACTION_H

#include "../GraphAction.hpp"

/**
 * Graph action that sets the animating state of nodes as it traverses the graph.
 */
class AnimateAction : public GraphAction
{
    public:

        virtual ~AnimateAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param animating Animating state this action applies to each visited node.
		 */
		AnimateAction(Handle<GraphNode> initNode, bool animating);

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        AnimateAction(const AnimateAction& copyFrom);
        AnimateAction& operator= (const AnimateAction& copyFrom);

		/// Animating state this action applies to each visited node.
		bool _animating;
};

#endif
