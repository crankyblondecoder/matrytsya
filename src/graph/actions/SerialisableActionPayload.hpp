#ifndef SERIALISABLE_ACTION_PAYLOAD_H
#define SERIALISABLE_ACTION_PAYLOAD_H

#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

// The following asserts are here to make sure hives across different architectures can accurately communicate.
static_assert(sizeof(float) == 4, "float must be 32-bit.");
static_assert(sizeof(double) == 8, "double must be 64-bit.");
static_assert(CHAR_BIT == 8, "This system must use 8-bit bytes.");

#include "../../util/RefCounted.hpp"

/**
 * Contains the serialised payload of a serialisable graph action.
 * @note All serialisation/de-serialisation is done using little endian order.
 *
 * The intended use of instances of this class is to serialise into the payload onlyu once or de-serialise out of the
 * payload only once.
 */
class SerialisableActionPayload : public RefCounted
{
	public:

		/**
		 * Constructor for serialising into the payload.
		 * @param sizeInBytes Size of payload in bytes, i.e. The sizeOf the data structure.
		 */
		SerialisableActionPayload(unsigned sizeInBytes);


		/**
		 * Constructor for setting a payload so it can be de-serialised.
		 * @param payload Payload to de-serialise from.
		 * @param sizeInBytes Number of bytes in payload.
		 */
		SerialisableActionPayload(std::byte* payload, unsigned sizeInBytes);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(uint8_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(int8_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(uint16_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(int16_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(uint32_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(int32_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(uint64_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(int64_t value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		 void serialiseValue(float value);

		/**
		 * Serialise a single value and add it to the payload.
		 */
		void serialiseValue(double value);

		/**
		 * Serialise a single value and add it to the payload.
		 * @note The value should only point to _byte_ array data only.
		 * @param value Byte array to serialise.
		 * @param sizeInBytes Number of bytes in array.
		 */
		void serialiseValue(const void* value, unsigned sizeInBytes);

		/**
		 * De-serialise a single value from the payload.
		 * @note De-serialisation reads from the end of the payload backward; call in reverse
		 *       order relative to the corresponding serialiseValue calls.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(uint8_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(int8_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(uint16_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(int16_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(uint32_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(int32_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(uint64_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(int64_t& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(float& value);

		/**
		 * De-serialise a single value from the payload.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(double& value);

		/**
		 * De-serialise a byte array from the payload.
		 * @note The value should only point to _byte_ array data only.
		 * @param value Buffer to write the de-serialised bytes into.
		 * @param sizeInBytes Number of bytes to read.
		 * @throw GraphException(OVERFLOW) if the payload has insufficient remaining bytes.
		 */
		void deserialiseValue(void* value, unsigned sizeInBytes);

		/**
		 * Get the serialised payload.
		 */
		std::span<std::byte> getPayload();

	protected:

		// This is a requirement of being ref counted.
		~SerialisableActionPayload();

	private:

		// Disable copying.
		SerialisableActionPayload(const SerialisableActionPayload& copyFrom);
		SerialisableActionPayload& operator= (const SerialisableActionPayload& copyFrom);

		/// Actual serialised payload.
		std::byte* _payload = 0;

		/// Size of the payload byte array.
		unsigned _payloadSize = 0;

		/**
		 * The current index being serialised into or de-serialised out of in/from the payload.
		 * For de-serialisation, it will always point at one index greater than the next byte to start de-serialisation
		 * from.
		 */
		unsigned _payloadPosn = 0;
};

#endif

