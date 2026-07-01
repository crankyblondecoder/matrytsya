#ifndef SERIALISABLE_ACTION_H
#define SERIALISABLE_ACTION_H

#include "../GraphAction.hpp"

class SerialisableActionPayload;

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
		 * Type of serialisable action.
		 * This is required to be able to re-create the action after teleportation.
		 */
		enum class SerialisableActionType
		{
			/// Just a place marker for initialisation.
			UNKNOWN,

			/// Ping action.
			PING
		};

		/**
		 * Get the type of serialisable action.
		 */
		virtual SerialisableActionType getSerialisbleType() = 0;

		/**
		 * Deserialise into this action from the given payload.
		 * @param data Payload to deserialise from.
		 */
		void deserialise(SerialisableActionPayload& data);

	protected:

		void _apply(GraphNode* target) override;

		void _complete() override;

		/**
		 * Serialise the implementing actions data into a series of bytes.
		 * @returns Payload that is a result of serialisation. Caller must de-reference this object once no longer
		 *          needed.
		 */
		virtual SerialisableActionPayload* _serialise() = 0;

		/**
		 * Deserialise into the implementing action from the given raw data.
		 */
		virtual void _deserialise(SerialisableActionPayload& data) = 0;

    private:
};

#endif
