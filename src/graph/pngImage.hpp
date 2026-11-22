#ifndef PNG_IMAGE_H
#define PNG_IMAGE_H

#include <string>

#include "ImageSource.hpp"

/**
 * Reading of the images scene nodes display. Embedded and file images must be PNG, which is checked by decoding
 * them in full; URL images are left to the viewer and so are not read at all.
 */
namespace pngImage
{
	/// Most bytes a PNG may take up.
	const std::size_t MAX_BYTES = 16 * 1024 * 1024;

	/// Most pixels a PNG may have along either side.
	const unsigned MAX_SIDE = 8192;

	/// Most pixels a PNG may have in all. Bounds the memory decoding it takes.
	const unsigned long MAX_PIXELS = 4096ul * 4096ul;

	/**
	 * Make an embedded image from PNG bytes.
	 * @param bytes PNG bytes.
	 * @returns The image, measured.
	 * @throw ImageException(NOT_PNG, DECODE_FAILED, TOO_LARGE) If the bytes are not a PNG that can be displayed.
	 */
	ImageSource fromPng(std::string bytes);

	/**
	 * Confine file images to a directory. Once set, a relative image path is resolved against this directory
	 * rather than the working directory, and only a file within it or a directory below it can be read, however
	 * the path is written and wherever any symbolic link along it leads.
	 * @note Meant to be called once at startup, before any hive is loaded, though it is safe to call at any time.
	 * @param directory Directory to confine file images to, or empty to lift any confinement.
	 * @returns False, leaving the root as it was, if directory does not exist or is not a directory.
	 */
	bool setFileRoot(const std::string& directory);

	/**
	 * @returns The directory file images are confined to, fully resolved, or empty if they are not confined.
	 */
	std::string getFileRoot();

	/**
	 * Make a file image by reading the PNG file at a path.
	 * @param path Path of the file. Relative to the image file root if one is set, or to the working directory
	 *        otherwise.
	 * @returns The image, measured, with path as given as its reference.
	 * @throw ImageException(OUTSIDE_FILE_ROOT) If an image file root is set and path names somewhere outside it.
	 * @throw ImageException(FILE_UNREADABLE) If the file cannot be read or is not a regular file, or if an image
	 *        file root is set and the file resolves through a symbolic link to somewhere outside it.
	 * @throw ImageException(NOT_PNG, DECODE_FAILED, TOO_LARGE) If the file is not a PNG that can be displayed.
	 */
	ImageSource fromFile(const std::string& path);

	/**
	 * Make a URL image. Nothing is fetched or checked, as the viewer loads it.
	 * @param url Address of the image.
	 * @returns The image, with url as its reference.
	 */
	ImageSource fromUrl(const std::string& url);

	/**
	 * @param url Address to check.
	 * @returns Whether url is an http or https address that a URL image may have.
	 */
	bool isImageUrl(const std::string& url);
}

#endif
