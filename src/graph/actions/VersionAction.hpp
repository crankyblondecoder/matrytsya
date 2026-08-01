#ifndef VERSION_ACTION_H
#define VERSION_ACTION_H

#include <atomic>

#include "../GraphAction.hpp"

/**
 * Graph action that generates a unique version of the sub-graph formed by the nodes it visits.
 */
class VersionAction : public GraphAction
{
    public:

        virtual ~VersionAction();

		VersionAction(Handle<GraphNode> initNode);

		/**
		 * Get the unique version of the sub-graph formed by the nodes visited by this action.
		 */
		unsigned getVersion();

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        VersionAction(const VersionAction& copyFrom);
        VersionAction& operator= (const VersionAction& copyFrom);

		/// Running sum of the versions of all nodes visited by this action.
		std::atomic<unsigned> _version{0};
};

#endif
