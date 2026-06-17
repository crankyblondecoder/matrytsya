#ifndef SERIALISABLE_ACTION_TARGET_H
#define SERIALISABLE_ACTION_TARGET_H

/**
 * Action target to use for a node that can (de)serialise an action.
 */
class SerialisableActionTarget
{
    public:

        virtual ~SerialisableActionTarget() {}

		SerialisableActionTarget() {}

		virtual bool ping() = 0;

	protected:

    private:
};

#endif
