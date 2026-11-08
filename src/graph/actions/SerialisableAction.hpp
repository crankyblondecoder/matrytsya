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

		/**
		 * @param initNode Initial node the new action is bound to.
		 * @param energy The energy that is assigned to the action.
		 * @param edgeTraversalKeys Keys this action holds, letting it traverse edges keyed with any of them. They
		 *        are carried in every payload this action serialises, so they survive teleportation.
		 */
		SerialisableAction(Handle<GraphNode>& initNode, unsigned energy, std::vector<unsigned> edgeTraversalKeys = {});

		/**
		 * Type of serialisable action.
		 * This is required to be able to re-create the action after teleportation.
		 */
		enum class SerialisableActionType
		{
			/// Just a place marker for initialisation.
			UNKNOWN,

			/// Ping action.
			PING,

			/// Trigger action.
			TRIGGER
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

		/**
		 * Read the edge traversal keys a serialised action held off the payload it was serialised into.
		 * @param data Payload to read from. Must not yet have had anything else deserialised from it, as the keys
		 *        are the last thing serialised and the payload reads back to front.
		 * @returns The keys, in the order the serialised action held them.
		 * @throw GraphException(OVERFLOW) If the payload holds fewer bytes than the keys need.
		 * @note Keys are fixed at an action's construction, so they are read here, ahead of constructing the
		 *       action, rather than as part of deserialise().
		 */
		static std::vector<unsigned> deserialiseEdgeTraversalKeys(SerialisableActionPayload& data);

	protected:

		bool _apply(GraphNode* target) override;

		/**
		 * Get the number of bytes this action's edge traversal keys take up in a payload, which every
		 * implementing _serialise() must add to the size of the payload it creates.
		 * @returns Size in bytes of the keys and their count.
		 */
		unsigned _getEdgeTraversalKeysSize();

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

		/**
		 * Append this action's edge traversal keys, then their count, to a payload the implementing action has
		 * already serialised its own data into.
		 * @param payload Payload to append to.
		 * @throw GraphException(OVERFLOW) If the implementing action did not size the payload to hold the keys.
		 */
		void __serialiseEdgeTraversalKeys(SerialisableActionPayload& payload);
};

#endif
