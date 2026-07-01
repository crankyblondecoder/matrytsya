#include "SerialisableActionPayload.hpp"

#include <bit>
#include <cstddef>
#include <cstring>
#include <span>

#include "../GraphException.hpp"

SerialisableActionPayload::~SerialisableActionPayload()
{
	if(_payload) delete[] _payload;
}

SerialisableActionPayload::SerialisableActionPayload(SerialisableAction::SerialisableActionType serialisableActionType,
		unsigned sizeInBytes)
{
	_serialisableActionType = serialisableActionType;
	_payload = new std::byte[sizeInBytes]{};
	_payloadSize = sizeInBytes;
}

SerialisableActionPayload::SerialisableActionPayload(SerialisableAction::SerialisableActionType serialisableActionType,
		std::byte* payload, unsigned sizeInBytes)
{
	_serialisableActionType = serialisableActionType;
	_payload = payload;
	_payloadSize = sizeInBytes;
	_payloadPosn = sizeInBytes;
}

void SerialisableActionPayload::serialiseValue(uint8_t value)
{
	if(_payloadPosn >= _payloadSize)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	_payload[_payloadPosn++] = static_cast<std::byte>(value);
}

void SerialisableActionPayload::serialiseValue(int8_t value)
{
	serialiseValue(static_cast<uint8_t>(value));
}

void SerialisableActionPayload::serialiseValue(uint16_t value)
{
	if(_payloadPosn + 2 > _payloadSize)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	_payload[_payloadPosn++] = static_cast<std::byte>(value);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 8);
}

void SerialisableActionPayload::serialiseValue(int16_t value)
{
	serialiseValue(static_cast<uint16_t>(value));
}

void SerialisableActionPayload::serialiseValue(uint32_t value)
{
	if(_payloadPosn + 4 > _payloadSize)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	_payload[_payloadPosn++] = static_cast<std::byte>(value);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 8);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 16);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 24);
}

void SerialisableActionPayload::serialiseValue(int32_t value)
{
	serialiseValue(static_cast<uint32_t>(value));
}

void SerialisableActionPayload::serialiseValue(uint64_t value)
{
	if(_payloadPosn + 8 > _payloadSize)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	_payload[_payloadPosn++] = static_cast<std::byte>(value);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 8);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 16);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 24);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 32);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 40);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 48);
	_payload[_payloadPosn++] = static_cast<std::byte>(value >> 56);
}

void SerialisableActionPayload::serialiseValue(int64_t value)
{
	serialiseValue(static_cast<uint64_t>(value));
}

void SerialisableActionPayload::serialiseValue(float value)
{
	serialiseValue(std::bit_cast<uint32_t>(value));
}

void SerialisableActionPayload::serialiseValue(double value)
{
	serialiseValue(std::bit_cast<uint64_t>(value));
}

void SerialisableActionPayload::serialiseValue(const void* value, unsigned sizeInBytes)
{
	if(_payloadPosn + sizeInBytes > _payloadSize)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	std::memcpy(_payload + _payloadPosn, value, sizeInBytes);

	_payloadPosn += sizeInBytes;
}

void SerialisableActionPayload::deserialiseValue(uint8_t& value)
{
	if(_payloadPosn < 1)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	value = static_cast<uint8_t>(_payload[--_payloadPosn]);
}

void SerialisableActionPayload::deserialiseValue(int8_t& value)
{
	uint8_t raw;
	deserialiseValue(raw);
	value = static_cast<int8_t>(raw);
}

void SerialisableActionPayload::deserialiseValue(uint16_t& value)
{
	if(_payloadPosn < 2)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	value  = static_cast<uint16_t>(_payload[--_payloadPosn]) << 8;
	value |= static_cast<uint16_t>(_payload[--_payloadPosn]);
}

void SerialisableActionPayload::deserialiseValue(int16_t& value)
{
	uint16_t raw;
	deserialiseValue(raw);
	value = static_cast<int16_t>(raw);
}

void SerialisableActionPayload::deserialiseValue(uint32_t& value)
{
	if(_payloadPosn < 4)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	value = static_cast<uint32_t>(_payload[--_payloadPosn]) << 24;
	value |= static_cast<uint32_t>(_payload[--_payloadPosn]) << 16;
	value |= static_cast<uint32_t>(_payload[--_payloadPosn]) << 8;
	value |= static_cast<uint32_t>(_payload[--_payloadPosn]);
}

void SerialisableActionPayload::deserialiseValue(int32_t& value)
{
	uint32_t raw;
	deserialiseValue(raw);
	value = static_cast<int32_t>(raw);
}

void SerialisableActionPayload::deserialiseValue(uint64_t& value)
{
	if(_payloadPosn < 8)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	value = static_cast<uint64_t>(_payload[--_payloadPosn]) << 56;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 48;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 40;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 32;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 24;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 16;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]) << 8;
	value |= static_cast<uint64_t>(_payload[--_payloadPosn]);
}

void SerialisableActionPayload::deserialiseValue(int64_t& value)
{
	uint64_t raw;
	deserialiseValue(raw);
	value = static_cast<int64_t>(raw);
}

void SerialisableActionPayload::deserialiseValue(float& value)
{
	uint32_t raw;
	deserialiseValue(raw);
	value = std::bit_cast<float>(raw);
}

void SerialisableActionPayload::deserialiseValue(double& value)
{
	uint64_t raw;
	deserialiseValue(raw);
	value = std::bit_cast<double>(raw);
}

void SerialisableActionPayload::deserialiseValue(void* value, unsigned sizeInBytes)
{
	if(_payloadPosn < sizeInBytes)
	{
		throw GraphException(GraphException::OVERFLOW);
	}

	_payloadPosn -= sizeInBytes;
	std::memcpy(value, _payload + _payloadPosn, sizeInBytes);
}

std::span<std::byte> SerialisableActionPayload::getPayload()
{
	if(_payload) return std::span<std::byte>(_payload, _payloadSize);

	// Effectively null.
	return std::span<std::byte> {};
}

SerialisableAction::SerialisableActionType SerialisableActionPayload::getSerialisableActionType()
{
	return _serialisableActionType;
}

