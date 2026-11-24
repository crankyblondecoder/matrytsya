#ifndef TEXT_CHANGE_ACTION_TARGET_H
#define TEXT_CHANGE_ACTION_TARGET_H

#include <string>

#include "ActionTarget.hpp"

/// How a text change combines the text it carries with the text a node already has.
enum class TextChangeOperation
{
	/// The carried text replaces the node's text.
	REPLACE,
	/// The carried text is added after the node's text.
	APPEND,
	/// The carried text is added before the node's text.
	PREPEND
};

/**
 * Action target to use for a node whose text can be changed by an action reaching it.
 */
class TextChangeActionTarget : virtual public ActionTarget
{
    public:

        virtual ~TextChangeActionTarget() {}

		TextChangeActionTarget() {}

		/**
		 * Ask this target to change its text.
		 * @param operation How text is combined with the target's current text.
		 * @param text Text the change carries.
		 * @note It is the target's to decide whether, and how, the change is made: it may apply it at once, hand
		 *       it to a script of its own or ignore it.
		 */
		virtual void requestTextChange(TextChangeOperation operation, const std::string& text) = 0;

	protected:

	private:
};

#endif
