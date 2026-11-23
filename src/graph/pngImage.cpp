#include "pngImage.hpp"

#include "ImageException.hpp"
#include "../thread/thread.hpp"
#include "../thread/ThreadMutex.hpp"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
	/// Guards the two spellings of the image file root below.
	ThreadMutex _rootLock;

	/// Image file root as it was given, made absolute and tidied but with any symbolic links in it left as they
	/// are, so that a path spelled through the same links is still taken to be inside it. Empty if unconfined.
	std::string _rootAsGiven;

	/// Image file root with every symbolic link in it resolved, which is what a file's resolved path is checked
	/// against. Empty if unconfined.
	std::string _rootResolved;

	// Tidies an absolute path without touching the filesystem: repeated separators and "." are dropped, and ".."
	// takes off the component before it, stopping at "/". Done so that whether a path is inside the root can be
	// decided from the path alone, before anything is looked up, and so says nothing about what exists outside.
	std::string normalise(const std::string& path)
	{
		std::vector<std::string> components;
		size_t start = 0;

		while(start <= path.size())
		{
			size_t end = path.find('/', start);

			if(end == std::string::npos) end = path.size();

			std::string component = path.substr(start, end - start);

			if(component == "..")
			{
				if(!components.empty()) components.pop_back();
			}
			else if(!component.empty() && component != ".")
			{
				components.push_back(component);
			}

			start = end + 1;
		}

		std::string result;

		for(const std::string& component : components) result += "/" + component;

		return result.empty() ? "/" : result;
	}

	// Whether path is root itself or lies somewhere below it. Both must be absolute and tidied.
	bool isWithin(const std::string& path, const std::string& root)
	{
		if(root == "/") return true;

		return path.compare(0, root.size(), root) == 0 && (path.size() == root.size() || path[root.size()] == '/');
	}

	// Works out which file to open for a path, enforcing the image file root if one is set.
	std::string resolveImagePath(const std::string& path, bool& confined)
	{
		std::string rootAsGiven;
		std::string rootResolved;

		{ SYNC(_rootLock)

			rootAsGiven = _rootAsGiven;
			rootResolved = _rootResolved;
		}

		confined = !rootResolved.empty();

		if(!confined) return path;

		if(path.empty()) throw ImageException(ImageException::FILE_UNREADABLE);

		std::string candidate = normalise(path[0] == '/' ? path : rootResolved + "/" + path);

		if(!isWithin(candidate, rootResolved) && !isWithin(candidate, rootAsGiven))
		{
			throw ImageException(ImageException::OUTSIDE_FILE_ROOT);
		}

		char* real = realpath(candidate.c_str(), nullptr);

		if(!real) throw ImageException(ImageException::FILE_UNREADABLE);

		std::string resolved(real);

		free(real);

		// A link inside the root that leads out of it is refused the same way as a file that is not there, so that
		// what lies outside the root cannot be learnt by planting links to it.
		if(!isWithin(resolved, rootResolved)) throw ImageException(ImageException::FILE_UNREADABLE);

		return resolved;
	}

	// Fills in an image's size and average colour from its PNG bytes, checking them as it goes. The whole image
	// is decoded rather than just its header, so that a truncated or corrupt file is turned away here rather than
	// being handed to every viewer.
	void measure(ImageSource& source)
	{
		const std::string& bytes = *source.png;

		if(bytes.size() > pngImage::MAX_BYTES) throw ImageException(ImageException::TOO_LARGE);

		if(bytes.size() < 8 || png_sig_cmp(reinterpret_cast<png_const_bytep>(bytes.data()), 0, 8) != 0)
		{
			throw ImageException(ImageException::NOT_PNG);
		}

		png_image image;

		memset(&image, 0, sizeof(image));
		image.version = PNG_IMAGE_VERSION;

		if(!png_image_begin_read_from_memory(&image, bytes.data(), bytes.size()))
		{
			png_image_free(&image);
			throw ImageException(ImageException::DECODE_FAILED);
		}

		if(image.width == 0 || image.height == 0 || image.width > pngImage::MAX_SIDE ||
			image.height > pngImage::MAX_SIDE ||
			static_cast<unsigned long>(image.width) * image.height > pngImage::MAX_PIXELS)
		{
			png_image_free(&image);
			throw ImageException(ImageException::TOO_LARGE);
		}

		image.format = PNG_FORMAT_RGBA;

		std::vector<png_byte> pixels(PNG_IMAGE_SIZE(image));

		if(!png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr))
		{
			png_image_free(&image);
			throw ImageException(ImageException::DECODE_FAILED);
		}

		// Colour is weighted by alpha so that the colour of fully transparent pixels, which is arbitrary, does not
		// tint the average.
		unsigned long long sums[3] = {0, 0, 0};
		unsigned long long alphaSum = 0;
		unsigned long pixelCount = static_cast<unsigned long>(image.width) * image.height;

		for(unsigned long pixel = 0; pixel < pixelCount; pixel++)
		{
			const png_byte* rgba = &pixels[pixel * 4];

			for(int channel = 0; channel < 3; channel++)
				sums[channel] += static_cast<unsigned long long>(rgba[channel]) * rgba[3];

			alphaSum += rgba[3];
		}

		for(int channel = 0; channel < 3; channel++)
		{
			source.averageColour[channel] = std::byte(alphaSum > 0 ? sums[channel] / alphaSum : 0);
		}

		source.averageColour[3] = std::byte(alphaSum / pixelCount);
		source.width = image.width;
		source.height = image.height;
	}
}

ImageSource pngImage::fromPng(std::string bytes)
{
	ImageSource source;

	source.kind = ImageSource::EMBEDDED;
	source.png = std::make_shared<const std::string>(std::move(bytes));

	measure(source);

	return source;
}

bool pngImage::setFileRoot(const std::string& directory)
{
	std::string asGiven;
	std::string resolved;

	if(!directory.empty())
	{
		std::string absolute = directory;

		if(absolute[0] != '/')
		{
			char cwd[PATH_MAX];

			if(!getcwd(cwd, sizeof(cwd))) return false;

			absolute = std::string(cwd) + "/" + absolute;
		}

		char* real = realpath(absolute.c_str(), nullptr);

		if(!real) return false;

		resolved = real;

		free(real);

		struct stat info;

		if(stat(resolved.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;

		asGiven = normalise(absolute);
	}

	{ SYNC(_rootLock)

		_rootAsGiven = asGiven;
		_rootResolved = resolved;
	}

	return true;
}

std::string pngImage::getFileRoot()
{
	{ SYNC(_rootLock)

		return _rootResolved;
	}
}

ImageSource pngImage::fromFile(const std::string& path)
{
	bool confined = false;
	std::string openPath = resolveImagePath(path, confined);

	// Confined, the path opened has every link already resolved, so refusing a link at the last step stops one
	// being swapped in after the check. Opened without blocking so that a FIFO cannot stall the load; it is then
	// refused below for not being a regular file.
	int fd = open(openPath.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | (confined ? O_NOFOLLOW : 0));

	if(fd < 0) throw ImageException(ImageException::FILE_UNREADABLE);

	struct stat info;

	if(fstat(fd, &info) != 0 || !S_ISREG(info.st_mode))
	{
		close(fd);
		throw ImageException(ImageException::FILE_UNREADABLE);
	}

	std::string bytes;
	char buffer[65536];
	ssize_t count;
	bool tooLarge = false;
	bool readFailed = false;

	while((count = read(fd, buffer, sizeof(buffer))) != 0)
	{
		if(count < 0)
		{
			if(errno == EINTR) continue;

			readFailed = true;
			break;
		}

		bytes.append(buffer, count);

		// Stopped early so that a file that is huge, or still growing, cannot exhaust memory.
		if(bytes.size() > MAX_BYTES)
		{
			tooLarge = true;
			break;
		}
	}

	close(fd);

	if(tooLarge) throw ImageException(ImageException::TOO_LARGE);
	if(readFailed) throw ImageException(ImageException::FILE_UNREADABLE);

	ImageSource source;

	source.kind = ImageSource::FILE;
	source.reference = path;
	source.png = std::make_shared<const std::string>(std::move(bytes));

	measure(source);

	return source;
}

ImageSource pngImage::fromUrl(const std::string& url)
{
	ImageSource source;

	source.kind = ImageSource::URL;
	source.reference = url;

	return source;
}

bool pngImage::isImageUrl(const std::string& url)
{
	auto hasPrefix = [&url](const char* prefix)
	{
		size_t length = strlen(prefix);

		// Something must follow the scheme for there to be anywhere to fetch from.
		return url.size() > length && strncasecmp(url.c_str(), prefix, length) == 0;
	};

	return hasPrefix("http://") || hasPrefix("https://");
}
