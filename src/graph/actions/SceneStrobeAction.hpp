#ifndef SCENE_STROBE_ACTION_H
#define SCENE_STROBE_ACTION_H

#include "ScriptAction.hpp"

/**
 * Graph action that strobes hive scene related nodes as it traverses the graph, in addition to invoking each
 * visited node's script.
 */
class SceneStrobeAction : public ScriptAction
{
    public:

        virtual ~SceneStrobeAction();

		SceneStrobeAction(GraphNodeHandle& initNode);

	protected:

		void _apply(GraphNode* target) override;

	private:

        // Do not allow copying.
        SceneStrobeAction(const SceneStrobeAction& copyFrom);
        SceneStrobeAction& operator= (const SceneStrobeAction& copyFrom);
};

#endif
