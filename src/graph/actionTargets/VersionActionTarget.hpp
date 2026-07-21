#ifndef VERSION_ACTION_TARGET_H
#define VERSION_ACTION_TARGET_H

#include "ActionTarget.hpp"

/**
 * Action target to use for a node that exposes a version number.
 */
class VersionActionTarget : virtual public ActionTarget
{
    public:

        virtual ~VersionActionTarget() {}

		VersionActionTarget() {}

		/**
		 * Get the version of this target.
		 * @returns The version number.
		 */
		virtual unsigned getVersion() = 0;

	protected:

    private:
};

#endif
