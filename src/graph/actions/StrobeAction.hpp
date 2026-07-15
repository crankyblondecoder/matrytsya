#ifndef STROBE_ACTION_H
#define STROBE_ACTION_H

#include "ScriptAction.hpp"

/**
 * Graph action that strobes nodes as it traverses the graph, in addition to invoking each
 * visited node's script.
 */
class StrobeAction : public ScriptAction
{
    public:

        virtual ~StrobeAction();

		StrobeAction(GraphHandle<GraphNode>& initNode);

	protected:

		void _apply(GraphNode* target) override;

	private:

        // Do not allow copying.
        StrobeAction(const StrobeAction& copyFrom);
        StrobeAction& operator= (const StrobeAction& copyFrom);
};

#endif
