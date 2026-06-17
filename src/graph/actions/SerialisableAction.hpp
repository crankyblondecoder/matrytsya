#ifndef SERIALISABLE_ACTION_H
#define SERIALISABLE_ACTION_H

#include <span>
#include <cstdint>

#include "../actionTargets/SerialisableActionTarget.hpp"
#include "../GraphAction.hpp"

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
		virtual std::span<uint8_t> _serialise() = 0;

		/**
		 * Deserialise into the implementing action from the given raw data.
		 */
		virtual void _deserialise(std::span<uint8_t> data) = 0;

    private:
};

#endif
