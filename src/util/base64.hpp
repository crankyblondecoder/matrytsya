#ifndef BASE64_H
#define BASE64_H

#include <string>

/**
 * Standard (RFC 4648) base64, with padding, used to carry binary data such as images in JSON.
 */
namespace base64
{
	/**
	 * Encode bytes as base64.
	 * @param bytes Bytes to encode.
	 * @returns The base64 text, padded to a multiple of 4 characters.
	 */
	std::string encode(const std::string& bytes);

	/**
	 * Decode base64 text. Whitespace is skipped, so that text wrapped over several lines decodes; anything else
	 * outside the base64 alphabet is refused.
	 * @param text Base64 text to decode. Padding is optional.
	 * @param bytes Set to the decoded bytes.
	 * @returns False, leaving bytes unspecified, if text is not valid base64.
	 */
	bool decode(const std::string& text, std::string& bytes);
}

#endif
