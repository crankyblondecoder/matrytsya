#ifndef SCENE_ACTION_H
#define SCENE_ACTION_H

#include "../GraphAction.hpp"

class GraphHiveSceneSurface;

/**
 * Graph action that populates a hive scene surface by traversing the graph.
 * @note The given surface is not owned by this action. The caller must ensure this action has sole use of it
 *       until the action completes.
 */
class SceneAction : public GraphAction
{
    public:

        virtual ~SceneAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param surface Surface this action populates as it visits nodes.
		 */
		SceneAction(GraphHandle<GraphNode> initNode, GraphHandle<GraphHiveSceneSurface> surface);

	protected:

		void _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        SceneAction(const SceneAction& copyFrom);
        SceneAction& operator= (const SceneAction& copyFrom);

		/// Surface this action populates as it traverses the graph. Not owned by this.
		GraphHandle<GraphHiveSceneSurface> _surface;
};

#endif
