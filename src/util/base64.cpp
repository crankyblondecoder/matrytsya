#include "base64.hpp"

namespace
{
	const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	// Value of a base64 character, -1 for padding, -2 for whitespace, or -3 for anything else.
	int valueOf(char character)
	{
		if(character >= 'A' && character <= 'Z') return character - 'A';
		if(character >= 'a' && character <= 'z') return character - 'a' + 26;
		if(character >= '0' && character <= '9') return character - '0' + 52;
		if(character == '+') return 62;
		if(character == '/') return 63;
		if(character == '=') return -1;
		if(character == ' ' || character == '\n' || character == '\r' || character == '\t') return -2;

		return -3;
	}
}

std::string base64::encode(const std::string& bytes)
{
	std::string text;

	text.reserve((bytes.size() + 2) / 3 * 4);

	for(std::size_t index = 0; index < bytes.size(); index += 3)
	{
		std::size_t remaining = bytes.size() - index;
		unsigned group = static_cast<unsigned char>(bytes[index]) << 16;

		if(remaining > 1) group |= static_cast<unsigned char>(bytes[index + 1]) << 8;
		if(remaining > 2) group |= static_cast<unsigned char>(bytes[index + 2]);

		text += ALPHABET[(group >> 18) & 0x3f];
		text += ALPHABET[(group >> 12) & 0x3f];
		text += remaining > 1 ? ALPHABET[(group >> 6) & 0x3f] : '=';
		text += remaining > 2 ? ALPHABET[group & 0x3f] : '=';
	}

	return text;
}

bool base64::decode(const std::string& text, std::string& bytes)
{
	bytes.clear();
	bytes.reserve(text.size() / 4 * 3);

	unsigned group = 0;
	int groupLength = 0;
	bool padded = false;

	for(char character : text)
	{
		int value = valueOf(character);

		if(value == -2) continue;
		if(value == -3) return false;

		if(value == -1)
		{
			padded = true;
			continue;
		}

		// Data after padding means the padding was not at the end.
		if(padded) return false;

		group = (group << 6) | value;
		groupLength++;

		if(groupLength == 4)
		{
			bytes += static_cast<char>((group >> 16) & 0xff);
			bytes += static_cast<char>((group >> 8) & 0xff);
			bytes += static_cast<char>(group & 0xff);
			group = 0;
			groupLength = 0;
		}
	}

	// A single character left over cannot hold a whole byte.
	if(groupLength == 1) return false;

	if(groupLength == 2)
	{
		bytes += static_cast<char>((group >> 4) & 0xff);
	}
	else if(groupLength == 3)
	{
		bytes += static_cast<char>((group >> 10) & 0xff);
		bytes += static_cast<char>((group >> 2) & 0xff);
	}

	return true;
}
