#ifndef IMAGE_SOURCE_H
#define IMAGE_SOURCE_H

#include <cstddef>
#include <memory>
#include <string>

/**
 * An image a scene node displays, along with where it came from.
 * @note An embedded or file image is held here as PNG bytes that have already been checked and measured. A URL
 *       image is only its URL: the viewer loads it, so it may be in any format the viewer can display, and its
 *       size and colour are not known here.
 */
struct ImageSource
{
	/// Where an image comes from.
	enum Kind
	{
		/// PNG bytes held in the node's own record.
		EMBEDDED,
		/// A PNG file on the machine running the hive.
		FILE,
		/// An http(s) URL that the viewer loads for itself.
		URL
	};

	/// Where the image comes from.
	Kind kind = EMBEDDED;

	/// Path of a FILE image or address of a URL image. Empty for an EMBEDDED image.
	std::string reference;

	/// PNG bytes of an EMBEDDED or FILE image. Null for a URL image. Shared rather than copied, as a scene
	/// copies its images every time it is collected and the bytes never change once read.
	std::shared_ptr<const std::string> png;

	/// Width in pixels. Zero if not known, as for a URL image.
	unsigned width = 0;

	/// Height in pixels. Zero if not known, as for a URL image.
	unsigned height = 0;

	/// Average colour of the image, weighted by alpha: R, G, B, A. Transparent if not known.
	std::byte averageColour[4] = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};

	/**
	 * @returns Whether both describe the same image from the same place. PNG bytes are compared by identity, as
	 *          bytes once read are never changed.
	 */
	bool operator== (const ImageSource& other) const
	{
		return kind == other.kind && reference == other.reference && png == other.png;
	}

	bool operator!= (const ImageSource& other) const
	{
		return !(*this == other);
	}
};

#endif
