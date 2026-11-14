#include "displayJson.hpp"

#include <stdio.h>

std::string jsonString(const std::string& value)
{
	std::string result = "\"";

	for(char character : value)
	{
		switch(character)
		{
			case '"':  result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;

			default:

				// Anything else below a space has no shorthand and may not appear raw, so goes out as an
				// escape. The cast keeps a high byte of a multi-byte UTF-8 sequence from looking negative
				// and being escaped as though it were a control character.
				if((unsigned char) character < 0x20)
				{
					char escape[7];

					snprintf(escape, sizeof(escape), "\\u%04x", (unsigned char) character);

					result += escape;
				}
				else
				{
					result += character;
				}

				break;
		}
	}

	return result + "\"";
}
