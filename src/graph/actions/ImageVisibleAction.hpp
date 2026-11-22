#ifndef IMAGE_VISIBLE_ACTION_H
#define IMAGE_VISIBLE_ACTION_H

#include "../GraphAction.hpp"

/**
 * Graph action that shows or hides the image of nodes as it traverses the graph.
 */
class ImageVisibleAction : public GraphAction
{
	public:

		virtual ~ImageVisibleAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param visible Image visibility this action applies to each visited node.
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them.
		 */
		ImageVisibleAction(Handle<GraphNode> initNode, bool visible, std::vector<unsigned> edgeTraversalKeys = {});

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

	private:

		// Do not allow copying.
		ImageVisibleAction(const ImageVisibleAction& copyFrom);
		ImageVisibleAction& operator= (const ImageVisibleAction& copyFrom);

		/// Image visibility this action applies to each visited node.
		bool _visible;
};

#endif
