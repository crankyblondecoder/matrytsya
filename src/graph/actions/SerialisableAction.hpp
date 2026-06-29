#ifndef SERIALISABLE_ACTION_H
#define SERIALISABLE_ACTION_H

#include "../actionTargets/SerialisableActionTarget.hpp"
#include "../GraphAction.hpp"
#include "SerialisableActionPayload.hpp"

/**
 * Graph action that is serialisable.
 */
class SerialisableAction : public GraphAction
{
    public:

        virtual ~SerialisableAction();

 		SerialisableAction(GraphNodeHandle& initNode, unsigned energy);

		unsigned long getFlag() override;

		/**
		 * Apply this action to a serialisable target.
		 */
		void apply(SerialisableActionTarget* target);

	protected:

		void _complete() override;

		/**
		 * Serialise the implementing actions data into a series of bytes.
		 */
		virtual SerialisableActionPayload* _serialise() = 0;

		/**
		 * Deserialise into the implementing action from the given raw data.
		 */
		virtual void _deserialise(SerialisableActionPayload& data) = 0;

    private:
};

#endif
