#ifndef TEXT_CHANGE_ACTION_H
#define TEXT_CHANGE_ACTION_H

#include <string>
#include <vector>

#include "../actionTargets/TextChangeActionTarget.hpp"
#include "../GraphAction.hpp"

/**
 * Graph action that asks the nodes it traverses to change their text.
 * @note Each node it reaches decides for itself whether and how the change is made. See TextChangeActionTarget.
 */
class TextChangeAction : public GraphAction
{
    public:

        virtual ~TextChangeAction();

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param operation How text is combined with each reached node's current text.
		 * @param text Text the change carries.
		 * @param nodeName If non-empty, only nodes with this name are asked to change their text.
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them.
		 */
		TextChangeAction(Handle<GraphNode> initNode, TextChangeOperation operation, std::string text,
			std::string nodeName = "", std::vector<unsigned> edgeTraversalKeys = {});

		/**
		 * Get the name an operation is known by to scripts.
		 * @param operation Operation to name.
		 * @returns "REPLACE", "APPEND" or "PREPEND".
		 */
		static const char* operationName(TextChangeOperation operation);

		/**
		 * Find the operation a name stands for.
		 * @param name "REPLACE", "APPEND" or "PREPEND".
		 * @param operation Set to the operation, if the name is one of those.
		 * @returns False, leaving operation unchanged, if the name is not one of those.
		 */
		static bool operationFromName(const char* name, TextChangeOperation& operation);

	protected:

		bool _apply(GraphNode* target) override;

		bool _starting() override;
		void _complete() override;

    private:

        // Do not allow copying.
        TextChangeAction(const TextChangeAction& copyFrom);
        TextChangeAction& operator= (const TextChangeAction& copyFrom);

		/// How _text is combined with each reached node's current text.
		TextChangeOperation _operation;

		/// Text the change carries.
		std::string _text;

		/// If non-empty, only nodes with this name are asked to change their text.
		std::string _nodeName;
};

#endif
