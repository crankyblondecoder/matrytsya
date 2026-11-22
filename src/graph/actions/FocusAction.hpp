#ifndef FOCUS_ACTION_H
#define FOCUS_ACTION_H

#include <string>

#include "../GraphAction.hpp"

/**
 * Graph action that asks the views of a scene to bring the visual objects of the nodes it reaches into focus.
 */
class FocusAction : public GraphAction
{
	public:

		virtual ~FocusAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param nodeName If non-empty, restricts focusing to nodes with this name.
		 * @param viewportFraction 0 to focus the way a right-click would, otherwise the fraction of the view's
		 *        height each node's bounds are fitted to. See FocusActionTarget::requestFocus().
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them.
		 */
		FocusAction(Handle<GraphNode> initNode, std::string nodeName = "", double viewportFraction = 0,
			std::vector<unsigned> edgeTraversalKeys = {});

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

	private:

		// Do not allow copying.
		FocusAction(const FocusAction& copyFrom);
		FocusAction& operator= (const FocusAction& copyFrom);

		/// If non-empty, only nodes with this name are focused on.
		std::string _nodeName;

		/// 0 to focus the way a right-click would, otherwise the fraction of the view's height to fit to.
		double _viewportFraction;
};

#endif
