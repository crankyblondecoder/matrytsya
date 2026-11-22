#ifndef IMAGE_EXCEPTION_H
#define IMAGE_EXCEPTION_H

#include "../util/Exception.hpp"

/**
 * Raised when an image cannot be read or is not one that can be displayed.
 */
class ImageException : public Exception
{
	public:

		enum Error
		{
			UNKNOWN,
			/// The bytes do not start with the PNG signature.
			NOT_PNG,
			/// The bytes start like a PNG but could not be decoded, e.g. they are truncated or corrupt.
			DECODE_FAILED,
			/// The image is larger than is allowed, in bytes or in pixels.
			TOO_LARGE,
			/// The image file could not be opened or read, was not a regular file, or, with an image file root set,
			/// resolved through a symbolic link to somewhere outside that root.
			FILE_UNREADABLE,
			/// An image file root is set, and the path given names somewhere outside it.
			OUTSIDE_FILE_ROOT
		};

		virtual ~ImageException(){}

		ImageException(Error error) : Exception(Exception::IMAGE), _error{error} {}

		/**
		 * @returns What went wrong.
		 */
		Error getError() const { return _error; }

		/**
		 * @returns A short description of what went wrong.
		 */
		const char* getMessage() const
		{
			switch(_error)
			{
				case NOT_PNG: return "image is not a PNG";
				case DECODE_FAILED: return "image could not be decoded as a PNG";
				case TOO_LARGE: return "image is too large";
				case FILE_UNREADABLE: return "image file could not be read";
				case OUTSIDE_FILE_ROOT: return "image file is outside the image file root directory";
				case UNKNOWN: break;
			}

			return "unknown image error";
		}

	private:

		Error _error;
};

#endif
