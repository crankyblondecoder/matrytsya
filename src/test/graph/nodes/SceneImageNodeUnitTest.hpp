#ifndef SCENE_IMAGE_NODE_UNIT_TEST_H
#define SCENE_IMAGE_NODE_UNIT_TEST_H

#include <gtest/gtest.h>

// The builder and persistence helpers these tests drive image nodes through.
#include "BuilderScriptNodeUnitTest.hpp"
#include "../../persist/HivePersistUnitTest.hpp"

#include "../../../graph/actions/SceneAction.hpp"
#include "../../../graph/GraphHive.hpp"
#include "../../../graph/GraphHiveSceneSurface.hpp"
#include "../../../graph/GraphNode.hpp"
#include "../../../graph/ImageException.hpp"
#include "../../../graph/pngImage.hpp"
#include "../../../graph/nodes/SceneImageNode.hpp"
#include "../../../graph/nodes/SceneRootNode.hpp"
#include "../../../graph/nodes/SceneTextScriptNode.hpp"
#include "../../../graph/nodes/SceneTransformNode.hpp"
#include "../../../graph/nodes/ScriptNode.hpp"
#include "../../../graph/nodes/ScriptSession.hpp"
#include "../../../persist/HiveBuilder.hpp"
#include "../../../persist/HiveNodeDescriptor.hpp"
#include "../../../persist/PersistException.hpp"
#include "../../../persist/json/JsonHiveLoader.hpp"
#include "../../../persist/json/JsonHiveWriter.hpp"
#include "../../../util/base64.hpp"
#include "../../../util/Handle.hpp"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{
	/**
	 * Encode RGBA pixels as a PNG.
	 * @param width Width in pixels.
	 * @param height Height in pixels.
	 * @param rgba Pixels, four bytes each, row by row.
	 * @returns The PNG bytes, or an empty string if libpng could not write them.
	 */
	std::string _imageTestPng(unsigned width, unsigned height, const std::vector<unsigned char>& rgba)
	{
		png_image image;

		memset(&image, 0, sizeof(image));
		image.version = PNG_IMAGE_VERSION;
		image.width = width;
		image.height = height;
		image.format = PNG_FORMAT_RGBA;

		png_alloc_size_t size = 0;

		if(!png_image_write_to_memory(&image, nullptr, &size, 0, rgba.data(), 0, nullptr)) return "";

		std::string bytes(size, '\0');

		if(!png_image_write_to_memory(&image, &bytes[0], &size, 0, rgba.data(), 0, nullptr)) return "";

		bytes.resize(size);

		return bytes;
	}

	/**
	 * @returns A 2x1 PNG: one opaque red pixel and one fully transparent blue one.
	 */
	std::string _imageTestRedPng()
	{
		return _imageTestPng(2, 1, {255, 0, 0, 255, 0, 0, 255, 0});
	}

	/**
	 * A file in a directory of its own, both removed when this goes out of scope.
	 */
	class ImageTestFile
	{
		public:

			/**
			 * @param bytes What the file holds.
			 */
			ImageTestFile(const std::string& bytes)
			{
				char dirTemplate[] = "/tmp/matrytsyaImageTestXXXXXX";

				char* dir = mkdtemp(dirTemplate);

				if(dir) _dir = dir;

				path = _dir + "/image.png";

				FILE* file = fopen(path.c_str(), "wb");

				if(file)
				{
					fwrite(bytes.data(), 1, bytes.size(), file);
					fclose(file);
				}
			}

			~ImageTestFile()
			{
				unlink(path.c_str());

				if(!_dir.empty()) rmdir(_dir.c_str());
			}

			/// Path of the file.
			std::string path;

		private:

			/// Directory the file is in.
			std::string _dir;
	};

	/**
	 * A directory tree to confine image files to, laid out as:
	 *   outside/secret.png       a PNG outside the root
	 *   root/top.png             a PNG at the top of the root
	 *   root/sub/deep.png        a PNG in a directory below it
	 *   root/sub/escape.png      a link to outside/secret.png
	 *   root/sub/inner.png       a link to root/top.png
	 *   root/sub/fifo.png        a FIFO
	 *   rootLink                 a link to root
	 * Everything is removed, and any image file root lifted, when this goes out of scope.
	 */
	class ImageTestRoot
	{
		public:

			ImageTestRoot(const std::string& png)
			{
				char dirTemplate[] = "/tmp/matrytsyaImageRootXXXXXX";

				char* dir = mkdtemp(dirTemplate);

				if(dir) base = dir;

				root = base + "/root";
				outside = base + "/outside";

				mkdir(root.c_str(), 0700);
				mkdir((root + "/sub").c_str(), 0700);
				mkdir(outside.c_str(), 0700);

				__write(outside + "/secret.png", png);
				__write(root + "/top.png", png);
				__write(root + "/sub/deep.png", png);

				symlink((outside + "/secret.png").c_str(), (root + "/sub/escape.png").c_str());
				symlink((root + "/top.png").c_str(), (root + "/sub/inner.png").c_str());
				mkfifo((root + "/sub/fifo.png").c_str(), 0600);
				symlink(root.c_str(), (base + "/rootLink").c_str());
			}

			~ImageTestRoot()
			{
				pngImage::setFileRoot("");

				const char* const files[] = {"/root/sub/deep.png", "/root/sub/escape.png", "/root/sub/inner.png",
					"/root/sub/fifo.png", "/root/top.png", "/outside/secret.png", "/rootLink"};

				for(const char* file : files) unlink((base + file).c_str());

				rmdir((root + "/sub").c_str());
				rmdir(root.c_str());
				rmdir(outside.c_str());
				rmdir(base.c_str());
			}

			/// Directory holding everything.
			std::string base;

			/// Directory to confine image files to.
			std::string root;

			/// Directory beside root, outside it.
			std::string outside;

		private:

			static void __write(const std::string& path, const std::string& bytes)
			{
				FILE* file = fopen(path.c_str(), "wb");

				if(!file) return;

				fwrite(bytes.data(), 1, bytes.size(), file);
				fclose(file);
			}
	};

	/**
	 * Run a scene action from the given root into the given surface.
	 */
	void _imageTestCollect(SceneRootNode* root, GraphHiveSceneSurface* surface)
	{
		Handle<GraphNode> rootHandle(root);
		SceneAction* sceneAction = new SceneAction(rootHandle, Handle<GraphHiveSceneSurface>(surface));

		sceneAction -> incrRef();
		sceneAction -> start();
		sceneAction -> waitOnComplete(0);
		sceneAction -> decrRef();
	}

	/**
	 * Expect a call to throw an ImageException with the given error.
	 */
	template<typename Call>
	void _expectImageError(Call call, ImageException::Error expected, const char* what)
	{
		try
		{
			call();
			ADD_FAILURE() << what << ": expected an ImageException";
		}
		catch(ImageException& exception)
		{
			EXPECT_EQ(exception.getError(), expected) << what;
		}
	}
}

// -- Reading images --

/**
 * A PNG is measured as it is read: its size, and its colour averaged by alpha, so a transparent pixel does not
 * tint it.
 */
TEST(SceneImageNodeTest, PngIsMeasured)
{
	std::string png = _imageTestRedPng();

	ASSERT_FALSE(png.empty());

	ImageSource source = pngImage::fromPng(png);

	EXPECT_EQ(source.kind, ImageSource::EMBEDDED);
	EXPECT_TRUE(source.reference.empty());
	ASSERT_TRUE(source.png);
	EXPECT_EQ(*source.png, png);
	EXPECT_EQ(source.width, 2u);
	EXPECT_EQ(source.height, 1u);
	EXPECT_EQ(std::to_integer<int>(source.averageColour[0]), 255);
	EXPECT_EQ(std::to_integer<int>(source.averageColour[1]), 0);
	EXPECT_EQ(std::to_integer<int>(source.averageColour[2]), 0);
	EXPECT_EQ(std::to_integer<int>(source.averageColour[3]), 127);
}

/**
 * Only a PNG is taken as an embedded image: other formats, a truncated PNG and one too large are all refused.
 */
TEST(SceneImageNodeTest, NonPngOrBrokenOrHugeImagesAreRefused)
{
	std::string png = _imageTestRedPng();

	const std::string jpeg("\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00", 20);
	const std::string gif = "GIF89a\x01\x00\x01\x00\x00\x00\x00;";

	_expectImageError([&]() { pngImage::fromPng(jpeg); }, ImageException::NOT_PNG, "JPEG");
	_expectImageError([&]() { pngImage::fromPng(gif); }, ImageException::NOT_PNG, "GIF");
	_expectImageError([&]() { pngImage::fromPng(""); }, ImageException::NOT_PNG, "empty");
	_expectImageError([&]() { pngImage::fromPng(png.substr(0, 40)); }, ImageException::DECODE_FAILED,
		"truncated");

	std::vector<unsigned char> wideRow((pngImage::MAX_SIDE + 1) * 4, 0);
	std::string wide = _imageTestPng(pngImage::MAX_SIDE + 1, 1, wideRow);

	ASSERT_FALSE(wide.empty());
	_expectImageError([&]() { pngImage::fromPng(wide); }, ImageException::TOO_LARGE, "too wide");
}

/**
 * A file image is read from its path, which it keeps as its reference; one that cannot be read is refused.
 */
TEST(SceneImageNodeTest, FileImageIsReadFromItsPath)
{
	std::string png = _imageTestRedPng();
	ImageTestFile file(png);

	ImageSource source = pngImage::fromFile(file.path);

	EXPECT_EQ(source.kind, ImageSource::FILE);
	EXPECT_EQ(source.reference, file.path);
	ASSERT_TRUE(source.png);
	EXPECT_EQ(*source.png, png);
	EXPECT_EQ(source.width, 2u);

	_expectImageError([&]() { pngImage::fromFile(file.path + ".missing"); }, ImageException::FILE_UNREADABLE,
		"missing file");

	ImageTestFile notPng("not a png at all");

	_expectImageError([&]() { pngImage::fromFile(notPng.path); }, ImageException::NOT_PNG, "text file");
}

/**
 * A URL image is not fetched or checked, so its format is not limited: whatever the address, as long as it is
 * http or https, is taken as given.
 */
TEST(SceneImageNodeTest, UrlImageIsTakenAsGivenWhateverItsFormat)
{
	const char* const accepted[] = {"https://example.org/photo.jpg", "http://example.org/anim.gif",
		"https://example.org/image", "HTTPS://example.org/vector.svg"};

	for(const char* url : accepted)
	{
		EXPECT_TRUE(pngImage::isImageUrl(url)) << url;

		ImageSource source = pngImage::fromUrl(url);

		EXPECT_EQ(source.kind, ImageSource::URL);
		EXPECT_EQ(source.reference, url);
		EXPECT_FALSE(source.png);
		EXPECT_EQ(source.width, 0u);
	}

	const char* const refused[] = {"", "http://", "ftp://example.org/a.png", "javascript:alert(1)",
		"data:image/png;base64,AAAA", "/local/path.png"};

	for(const char* url : refused) EXPECT_FALSE(pngImage::isImageUrl(url)) << url;
}

/**
 * Base64 round trips any bytes, and anything outside the alphabet is refused.
 */
TEST(SceneImageNodeTest, Base64RoundTripsAndRefusesGarbage)
{
	std::string bytes;

	for(int value = 0; value < 256; value++) bytes += static_cast<char>(value);

	for(std::size_t length = 0; length < 6; length++)
	{
		std::string decoded;

		ASSERT_TRUE(base64::decode(base64::encode(bytes.substr(0, length)), decoded)) << length;
		EXPECT_EQ(decoded, bytes.substr(0, length));
	}

	std::string decoded;

	ASSERT_TRUE(base64::decode(base64::encode(bytes), decoded));
	EXPECT_EQ(decoded, bytes);

	EXPECT_TRUE(base64::decode("aGVs\nbG8=", decoded));
	EXPECT_EQ(decoded, "hello");

	EXPECT_FALSE(base64::decode("aGVs*bG8=", decoded));
	EXPECT_FALSE(base64::decode("aG=Vs", decoded));
	EXPECT_FALSE(base64::decode("a", decoded));
}

// -- The node and the scene --

/**
 * An image node reports a type of its own.
 */
TEST(SceneImageNodeTest, ReportsSceneImageNodeType)
{
	SceneImageNode* node = new SceneImageNode();

	EXPECT_EQ(node -> getType(), GraphNodeType::SCENE_IMAGE_NODE);
	EXPECT_EQ(GraphNode::typeFromName("SCENE_IMAGE_NODE"), GraphNodeType::SCENE_IMAGE_NODE);

	node -> decrRef();
}

/**
 * An image beneath a transform reaches the scene with where it is put, its height, its box and that transform, and
 * its bytes can then be had from the surface by node id. A URL image has no bytes to hand out.
 */
TEST(SceneImageNodeTest, ImageReachesScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTransformNode* transformNode = new SceneTransformNode();
	SceneImageNode* imageNode = new SceneImageNode();
	SceneImageNode* urlNode = new SceneImageNode();

	Transform translation = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};
	transformNode -> setTransform(translation);

	ImageSource source = pngImage::fromPng(_imageTestRedPng());
	double position[3] = {1, 2, 3};
	TextBackground background;
	background.padding = 0.25;

	imageNode -> setSource(source);
	imageNode -> setPosition(position);
	imageNode -> setHeight(2.5);
	imageNode -> setBackground(background);

	urlNode -> setSource(pngImage::fromUrl("https://example.org/photo.jpg"));

	hive -> addNode(root);
	hive -> addNode(transformNode);
	hive -> addNode(imageNode);
	hive -> addNode(urlNode);

	// A chain, as an action takes one path through a graph rather than every branch of it.
	Handle<GraphNode> urlHandle(urlNode);
	Handle<GraphNode> transformHandle(transformNode);
	Handle<GraphNode> imageHandle(imageNode);

	root -> createEdge(urlHandle, {});
	urlNode -> createEdge(transformHandle, {});
	transformNode -> createEdge(imageHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	_imageTestCollect(root, surface);

	GraphHiveSceneSurface::Scene scene = surface -> getScene();

	ASSERT_EQ(scene.images.size(), 2u);

	const GraphHiveSceneSurface::Image* placed = nullptr;
	const GraphHiveSceneSurface::Image* remote = nullptr;

	for(const GraphHiveSceneSurface::Image& image : scene.images)
	{
		if(image.nodeId == imageNode -> getId()) placed = &image;
		if(image.nodeId == urlNode -> getId()) remote = &image;
	}

	ASSERT_NE(placed, nullptr);
	ASSERT_NE(remote, nullptr);

	EXPECT_EQ(placed -> version, imageNode -> getVersion());
	EXPECT_DOUBLE_EQ(placed -> position[2], 3);
	EXPECT_DOUBLE_EQ(placed -> height, 2.5);
	EXPECT_DOUBLE_EQ(placed -> background.padding, 0.25);
	EXPECT_EQ(placed -> source.width, 2u);
	EXPECT_EQ(scene.modelTransforms[placed -> modelTransformIndex].transform[12], 5);

	EXPECT_EQ(remote -> source.kind, ImageSource::URL);
	EXPECT_EQ(remote -> source.reference, "https://example.org/photo.jpg");

	std::shared_ptr<const std::string> png;

	ASSERT_TRUE(surface -> getImagePng(imageNode -> getId(), png));
	EXPECT_EQ(png, source.png) << "The bytes should be shared, not copied.";
	EXPECT_FALSE(surface -> getImagePng(urlNode -> getId(), png));
	EXPECT_FALSE(surface -> getImagePng(root -> getId(), png));

	surface -> close();

	hive -> shutdown();
}

/**
 * A hidden image is left out of the scene, and shown again it comes back.
 */
TEST(SceneImageNodeTest, HiddenImageIsLeftOutOfScene)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneImageNode* imageNode = new SceneImageNode();

	imageNode -> setSource(pngImage::fromUrl("https://example.org/a.webp"));
	imageNode -> setVisible(false);

	hive -> addNode(root);
	hive -> addNode(imageNode);

	Handle<GraphNode> imageHandle(imageNode);

	root -> createEdge(imageHandle, {});

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(Handle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	_imageTestCollect(root, surface);
	EXPECT_TRUE(surface -> getScene().images.empty());

	imageNode -> setVisible(true);

	_imageTestCollect(root, surface);
	EXPECT_EQ(surface -> getScene().images.size(), 1u);

	surface -> close();

	hive -> shutdown();
}

/**
 * Only an actual change bumps the version, so setting what is already there does not make a surface repopulate.
 */
TEST(SceneImageNodeTest, OnlyAnActualChangeBumpsTheVersion)
{
	SceneImageNode* node = new SceneImageNode();

	ImageSource source = pngImage::fromUrl("https://example.org/a.png");
	double position[3] = {1, 2, 3};

	node -> setSource(source);
	node -> setPosition(position);
	node -> setHeight(2);
	node -> setVisible(false);

	unsigned version = node -> getVersion();

	node -> setSource(source);
	node -> setPosition(position);
	node -> setHeight(2);
	node -> setVisible(false);
	node -> setBackground(TextBackground());

	EXPECT_EQ(node -> getVersion(), version);

	EXPECT_FALSE(node -> setHeight(0));
	EXPECT_FALSE(node -> setHeight(-1));
	EXPECT_DOUBLE_EQ(node -> getHeight(), 2);
	EXPECT_EQ(node -> getVersion(), version);

	node -> setSource(pngImage::fromUrl("https://example.org/b.png"));

	EXPECT_NE(node -> getVersion(), version);

	node -> decrRef();
}

/**
 * A script's IMAGE_VISIBLE action shows or hides the images it reaches, passing over nodes that hold none. It
 * leaves text alone, as TEXT_VISIBLE leaves images alone.
 */
TEST(SceneImageNodeTest, ImageVisibleActionShowsAndHidesImagesOnly)
{
	GraphHive* hive = new GraphHive(2);
	Handle<GraphHive> hiveHandle(hive);

	ScriptNode* hideImages = new ScriptNode("emitAction(ActionType.IMAGE_VISIBLE, { visible = false })", "");
	ScriptNode* hideTexts = new ScriptNode("emitAction(ActionType.TEXT_VISIBLE, { visible = false })", "");
	ScriptNode* middleNode = new ScriptNode("", "");
	SceneImageNode* imageNode = new SceneImageNode();
	SceneTextScriptNode* textNode = new SceneTextScriptNode("", "");

	hive -> addNode(hideImages);
	hive -> addNode(hideTexts);
	hive -> addNode(middleNode);
	hive -> addNode(imageNode);
	hive -> addNode(textNode);

	// A chain, as an action takes one path through a graph rather than every branch of it.
	Handle<GraphNode> middleHandle(middleNode);
	Handle<GraphNode> imageHandle(imageNode);
	Handle<GraphNode> textHandle(textNode);

	hideImages -> createEdge(middleHandle, {});
	hideTexts -> createEdge(middleHandle, {});
	middleNode -> createEdge(imageHandle, {});
	imageNode -> createEdge(textHandle, {});

	{ Handle<ScriptSession> sessionHandle = hideTexts -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	hive -> waitOnNoActionsActive(0);

	EXPECT_TRUE(imageNode -> getVisible()) << "TEXT_VISIBLE should not reach into images.";
	EXPECT_FALSE(textNode -> getVisible());

	{ Handle<ScriptSession> sessionHandle = hideImages -> requestCoreSession();

		ASSERT_TRUE(sessionHandle.getInstance() -> run());
	}

	hive -> waitOnNoActionsActive(0);

	EXPECT_FALSE(imageNode -> getVisible());

	hive -> shutdown();
}

/**
 * A malformed IMAGE_VISIBLE call raises a Lua error.
 */
TEST(SceneImageNodeTest, MalformedImageVisibleCallsRaise)
{
	const char* const badCalls[] = {
		"emitAction(ActionType.IMAGE_VISIBLE)",
		"emitAction(ActionType.IMAGE_VISIBLE, {})",
		"emitAction(ActionType.IMAGE_VISIBLE, { visible = 0 })",
		"emitAction(ActionType.IMAGE_VISIBLE, { visible = true, extra = 1 })"};

	for(const char* call : badCalls)
	{
		GraphHive* hive = new GraphHive(2);
		Handle<GraphHive> hiveHandle(hive);

		ScriptNode* node = new ScriptNode(call, "");

		hive -> addNode(node);

		{ Handle<ScriptSession> sessionHandle = node -> requestCoreSession();

			EXPECT_FALSE(sessionHandle.getInstance() -> run()) << call;
		}

		hive -> shutdown();
	}
}

// -- Hive definitions --

/**
 * Each of the three image sources is parsed from a hive definition, along with where the image goes and how it is
 * shown.
 */
TEST(SceneImageNodeTest, JsonImageSourcesAreParsed)
{
	std::string png = _imageTestRedPng();

	HiveNodeDescriptor embedded = JsonHiveLoader::parseNodeFragment(
		R"({ "type": "SceneImageNode", "name": "a", "imageData": ")" + base64::encode(png) + R"(",
			"imagePosition": [1, 2, 3], "imageHeight": 4, "imageVisible": false,
			"imageBackground": { "padding": 0.5, "cornerRadius": 0.25 } })");

	EXPECT_EQ(embedded.type, HiveNodeDescriptor::SCENE_IMAGE);
	EXPECT_EQ(embedded.imageKind, ImageSource::EMBEDDED);
	EXPECT_EQ(embedded.imageBytes, png);
	EXPECT_DOUBLE_EQ(embedded.imagePosition[1], 2);
	EXPECT_DOUBLE_EQ(embedded.imageHeight, 4);
	EXPECT_FALSE(embedded.imageVisible);
	EXPECT_DOUBLE_EQ(embedded.imageBackground.padding, 0.5);
	EXPECT_DOUBLE_EQ(embedded.imageBackground.cornerRadii[TextBackground::BOTTOM_LEFT], 0.25);

	HiveNodeDescriptor file = JsonHiveLoader::parseNodeFragment(
		R"({ "type": "SceneImageNode", "name": "b", "imageFile": "images/logo.png" })");

	EXPECT_EQ(file.imageKind, ImageSource::FILE);
	EXPECT_EQ(file.imageReference, "images/logo.png");
	EXPECT_DOUBLE_EQ(file.imageHeight, 1);
	EXPECT_TRUE(file.imageVisible);

	HiveNodeDescriptor url = JsonHiveLoader::parseNodeFragment(
		R"({ "type": "SceneImageNode", "name": "c", "imageUrl": "https://example.org/photo.jpg" })");

	EXPECT_EQ(url.imageKind, ImageSource::URL);
	EXPECT_EQ(url.imageReference, "https://example.org/photo.jpg");
}

/**
 * A hive definition's image node must have exactly one well formed source, and well formed fields besides.
 */
TEST(SceneImageNodeTest, MalformedJsonImageFieldsThrow)
{
	struct Case
	{
		const char* fields;
		PersistException::Error expected;
	};

	const Case cases[] = {
		{"", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageFile": "a.png", "imageUrl": "https://example.org/a.png")", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageUrl": "ftp://example.org/a.png")", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageUrl": 7)", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageFile": "")", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageData": "not*base64")", PersistException::JSON_INVALID_IMAGE_SOURCE},
		{R"("imageFile": "a.png", "imageHeight": 0)", PersistException::JSON_INVALID_IMAGE_HEIGHT},
		{R"("imageFile": "a.png", "imagePosition": [1, 2])", PersistException::JSON_INVALID_IMAGE_POSITION},
		{R"("imageFile": "a.png", "imageVisible": 1)", PersistException::JSON_INVALID_IMAGE_VISIBLE},
		{R"("imageFile": "a.png", "imageBackground": { "padding": -1 })", PersistException::JSON_INVALID_IMAGE_BACKGROUND}
	};

	for(const Case& testCase : cases)
	{
		std::string fragment = std::string(R"({ "type": "SceneImageNode", "name": "a")") +
			(testCase.fields[0] ? ", " : "") + testCase.fields + " }";

		try
		{
			JsonHiveLoader::parseNodeFragment(fragment);
			ADD_FAILURE() << "Expected a PersistException for " << fragment;
		}
		catch(PersistException& exception)
		{
			EXPECT_EQ(exception.getError(), testCase.expected) << fragment;
		}
	}
}

/**
 * End-to-end: a hive definition's embedded, file and URL images all build into image nodes, and an embedded image
 * that is not a PNG, or a file that is missing, fails the build.
 */
TEST(SceneImageNodeTest, EndToEndImagesBuildFromJson)
{
	std::string png = _imageTestRedPng();
	ImageTestFile file(png);

	std::string json = R"({
		"name": "ImageHive",
		"nodes": [
			{ "type": "SceneImageNode", "name": "embedded", "imageData": ")" + base64::encode(png) + R"(",
				"imageHeight": 3 },
			{ "type": "SceneImageNode", "name": "file", "imageFile": ")" + file.path + R"(" },
			{ "type": "SceneImageNode", "name": "url", "imageUrl": "https://example.org/photo" }
		]
	})";

	JsonHiveLoader loader(json);

	GraphHive* hive = HiveBuilder::build(loader, 2);
	Handle<GraphHive> hiveHandle(hive);

	for(const char* name : {"embedded", "file", "url"})
	{
		Handle<GraphNode> handle = hive -> getNode(name);

		ASSERT_TRUE(handle.isValid()) << name;
		EXPECT_EQ(handle.getInstance() -> getType(), GraphNodeType::SCENE_IMAGE_NODE) << name;
	}

	SceneImageNode* embedded = dynamic_cast<SceneImageNode*>(hive -> getNode("embedded").getInstance());
	SceneImageNode* fromFile = dynamic_cast<SceneImageNode*>(hive -> getNode("file").getInstance());

	ASSERT_NE(embedded, nullptr);
	ASSERT_NE(fromFile, nullptr);

	EXPECT_EQ(embedded -> getSource().width, 2u);
	EXPECT_DOUBLE_EQ(embedded -> getHeight(), 3);
	EXPECT_EQ(fromFile -> getSource().reference, file.path);

	hive -> shutdown();

	const std::string broken[] = {
		R"({ "name": "Broken", "nodes": [ { "type": "SceneImageNode", "name": "a", "imageData": ")" +
			base64::encode("GIF89a") + R"(" } ] })",
		R"({ "name": "Broken", "nodes": [ { "type": "SceneImageNode", "name": "a", "imageFile": ")" +
			file.path + R"(.missing" } ] })"};

	for(const std::string& brokenJson : broken)
	{
		JsonHiveLoader brokenLoader(brokenJson);

		try
		{
			GraphHive* brokenHive = HiveBuilder::build(brokenLoader, 2);
			brokenHive -> shutdown();
			brokenHive -> decrRef();
			ADD_FAILURE() << "Expected the build to fail";
		}
		catch(PersistException& exception)
		{
			EXPECT_EQ(exception.getError(), PersistException::INVALID_IMAGE);
			EXPECT_FALSE(exception.getDetail().empty());
		}
	}
}

/**
 * Each image source is written out as it was given: an embedded image as its bytes, a file or URL image as its
 * reference. What is written reads back the same.
 */
TEST(SceneImageNodeTest, JsonWriterRoundTripsEveryImageSource)
{
	HiveNodeDescriptor embedded{};
	embedded.type = HiveNodeDescriptor::SCENE_IMAGE;
	embedded.name = "embedded";
	embedded.imageKind = ImageSource::EMBEDDED;
	embedded.imageBytes = _imageTestRedPng();
	embedded.imagePosition[0] = 1.5;
	embedded.imageHeight = 0.75;
	embedded.imageBackground.fillColour[3] = std::byte{9};
	embedded.imageVisible = false;

	HiveNodeDescriptor file{};
	file.type = HiveNodeDescriptor::SCENE_IMAGE;
	file.name = "file";
	file.imageKind = ImageSource::FILE;
	file.imageReference = "images/logo.png";

	HiveNodeDescriptor url{};
	url.type = HiveNodeDescriptor::SCENE_IMAGE;
	url.name = "url";
	url.imageKind = ImageSource::URL;
	url.imageReference = "https://example.org/photo.jpg";

	for(const HiveNodeDescriptor& written : {embedded, file, url})
	{
		std::string fragment = JsonHiveWriter::nodeFragment(written);

		HiveNodeDescriptor read = JsonHiveLoader::parseNodeFragment(fragment);

		EXPECT_EQ(JsonHiveWriter::nodeFragment(read), fragment) << written.name;
		EXPECT_EQ(read.imageKind, written.imageKind) << written.name;
		EXPECT_EQ(read.imageBytes, written.imageBytes) << written.name;
		EXPECT_EQ(read.imageReference, written.imageReference) << written.name;
		EXPECT_EQ(read.imageVisible, written.imageVisible) << written.name;
	}

	EXPECT_EQ(JsonHiveWriter::nodeFragment(file).find("imageData"), std::string::npos)
		<< "A file image should be written as its path, not its bytes.";
	EXPECT_EQ(JsonHiveWriter::nodeFragment(url).find("imageData"), std::string::npos);
}

/**
 * A hive holding every kind of image is stored in LMDB and rebuilt from there the same: an embedded image keeps
 * its bytes, and file and URL images keep their references.
 */
TEST(SceneImageNodeTest, LmdbStoresImagesAndRebuildsThem)
{
	std::string png = _imageTestRedPng();
	ImageTestFile file(png);

	std::string json = R"({
		"name": "ImageHive",
		"nodes": [
			{ "type": "SceneRootNode", "name": "root", "edges": [ { "toNodeName": "embedded" },
				{ "toNodeName": "file" }, { "toNodeName": "url" } ] },
			{ "type": "SceneImageNode", "name": "embedded", "imageData": ")" + base64::encode(png) + R"(" },
			{ "type": "SceneImageNode", "name": "file", "imageFile": ")" + file.path + R"(" },
			{ "type": "SceneImageNode", "name": "url", "imageUrl": "https://example.org/photo.jpg" }
		]
	})";

	PersistTestDatabase database;
	Handle<LmdbHiveStore> store = _openPersistTestStore(database.path);

	JsonHiveLoader jsonLoader(json);

	GraphHive* hive = HiveBuilder::build(jsonLoader, 2);
	Handle<GraphHive> hiveHandle(hive);

	Handle<HivePersister> persister = _createPersistTestPersister(store, hiveHandle, jsonLoader);

	persister.getInstance() -> flush();

	LmdbHiveLoader stored(*store.getInstance(), "ImageHive");

	ASSERT_EQ(stored.getNodeCount(), jsonLoader.getNodeCount());

	for(unsigned i = 0; i < jsonLoader.getNodeCount(); i++)
	{
		EXPECT_EQ(JsonHiveWriter::nodeFragment(stored.getNode(i)), JsonHiveWriter::nodeFragment(jsonLoader.getNode(i)))
			<< jsonLoader.getNode(i).name;
	}

	EXPECT_EQ(_findPersistTestNode(stored, "embedded").imageBytes, png);
	EXPECT_EQ(_findPersistTestNode(stored, "file").imageReference, file.path);
	EXPECT_EQ(_findPersistTestNode(stored, "url").imageReference, "https://example.org/photo.jpg");

	GraphHive* rebuilt = HiveBuilder::build(stored, 2);
	Handle<GraphHive> rebuiltHandle(rebuilt);

	SceneImageNode* rebuiltEmbedded = dynamic_cast<SceneImageNode*>(rebuilt -> getNode("embedded").getInstance());

	ASSERT_NE(rebuiltEmbedded, nullptr);
	ASSERT_TRUE(rebuiltEmbedded -> getSource().png);
	EXPECT_EQ(*rebuiltEmbedded -> getSource().png, png);

	persister.getInstance() -> stop();

	hive -> shutdown();
	rebuilt -> shutdown();
}

// -- Builder scripts --

/**
 * createSceneImageNode adds an image node from any of the three sources. An image that cannot be displayed is
 * reported back rather than raised, and does not use the build up; a malformed call raises.
 */
TEST(SceneImageNodeTest, BuilderCreatesImageNodes)
{
	std::string png = _imageTestRedPng();
	ImageTestFile file(png);

	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		local function raises(f, ...) return not pcall(f, ...) end

		brokenOk, brokenReason = createSceneImageNode{ name = "broken", imageData = ")" + base64::encode("GIF89a") +
			R"(" }
		brokenReason = brokenReason or ""

		noSourceRaised = raises(createSceneImageNode, { name = "x" })
		twoSourcesRaised = raises(createSceneImageNode, { name = "x", imageFile = "a.png",
			imageUrl = "https://example.org/a.png" })
		badUrlRaised = raises(createSceneImageNode, { name = "x", imageUrl = "ftp://example.org/a.png" })
		badHeightRaised = raises(createSceneImageNode, { name = "x", imageUrl = "https://example.org/a.png",
			imageHeight = 0 })

		embeddedOk = createSceneImageNode{ name = "embedded", imageData = ")" + base64::encode(png) + R"(",
			imagePosition = {1, 2, 3}, imageHeight = 2, imageVisible = false,
			imageBackground = { padding = 0.5 } }
		fileOk = createSceneImageNode{ name = "file", imageFile = ")" + file.path + R"(" }
		urlOk = createSceneImageNode{ name = "url", imageUrl = "https://example.org/photo.jpg" }
	)", "", builder, false);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_FALSE(_coreBool(builder, "brokenOk"));
	EXPECT_NE(_coreString(builder, "brokenReason").find("not a PNG"), std::string::npos)
		<< _coreString(builder, "brokenReason");

	for(const char* name : {"noSourceRaised", "twoSourcesRaised", "badUrlRaised", "badHeightRaised", "embeddedOk",
		"fileOk", "urlOk"})
	{
		EXPECT_TRUE(_coreBool(builder, name)) << name;
	}

	EXPECT_FALSE(hive -> getNode("broken").isValid());

	SceneImageNode* embedded = dynamic_cast<SceneImageNode*>(hive -> getNode("embedded").getInstance());
	SceneImageNode* fromFile = dynamic_cast<SceneImageNode*>(hive -> getNode("file").getInstance());
	SceneImageNode* url = dynamic_cast<SceneImageNode*>(hive -> getNode("url").getInstance());

	ASSERT_NE(embedded, nullptr);
	ASSERT_NE(fromFile, nullptr);
	ASSERT_NE(url, nullptr);

	EXPECT_EQ(embedded -> getSource().width, 2u);
	EXPECT_DOUBLE_EQ(embedded -> getHeight(), 2);
	EXPECT_FALSE(embedded -> getVisible());
	EXPECT_DOUBLE_EQ(embedded -> getBackground().padding, 0.5);
	EXPECT_EQ(fromFile -> getSource().kind, ImageSource::FILE);
	EXPECT_EQ(url -> getSource().reference, "https://example.org/photo.jpg");

	hive -> shutdown();
}

// -- Confining image files to a root --

/**
 * A root must be an existing directory; anything else is refused and leaves the root as it was.
 */
TEST(SceneImageNodeTest, FileRootMustBeADirectory)
{
	ImageTestRoot tree(_imageTestRedPng());

	EXPECT_FALSE(pngImage::setFileRoot(tree.base + "/missing"));
	EXPECT_FALSE(pngImage::setFileRoot(tree.root + "/top.png"));
	EXPECT_TRUE(pngImage::getFileRoot().empty());

	ASSERT_TRUE(pngImage::setFileRoot(tree.root));
	EXPECT_FALSE(pngImage::setFileRoot(tree.base + "/missing"));

	char* real = realpath(tree.root.c_str(), nullptr);
	ASSERT_NE(real, nullptr);
	EXPECT_EQ(pngImage::getFileRoot(), std::string(real));
	free(real);

	ASSERT_TRUE(pngImage::setFileRoot(""));
	EXPECT_TRUE(pngImage::getFileRoot().empty());
}

/**
 * With a root set, files in it and below it are read, by a path relative to it or an absolute one, including
 * through a link that stays inside it. Each keeps the path it was given as its reference.
 */
TEST(SceneImageNodeTest, FileRootAdmitsFilesWithinIt)
{
	std::string png = _imageTestRedPng();
	ImageTestRoot tree(png);

	ASSERT_TRUE(pngImage::setFileRoot(tree.root));

	const std::string admitted[] = {"top.png", "sub/deep.png", "./sub/../top.png", "sub/inner.png",
		tree.root + "/sub/deep.png"};

	for(const std::string& path : admitted)
	{
		try
		{
			ImageSource source = pngImage::fromFile(path);

			EXPECT_EQ(source.reference, path);
			ASSERT_TRUE(source.png) << path;
			EXPECT_EQ(*source.png, png) << path;
		}
		catch(ImageException& exception)
		{
			ADD_FAILURE() << path << ": " << exception.getMessage();
		}
	}

	// A root given through a link admits paths spelled either way.
	ASSERT_TRUE(pngImage::setFileRoot(tree.base + "/rootLink"));

	EXPECT_NO_THROW(pngImage::fromFile(tree.base + "/rootLink/top.png"));
	EXPECT_NO_THROW(pngImage::fromFile(tree.root + "/top.png"));
	EXPECT_NO_THROW(pngImage::fromFile("sub/deep.png"));
}

/**
 * With a root set, a path out of it is refused before anything is looked up, and a link out of it is refused as
 * if the file could not be read. An absolute path must name the root the way it was given or as it resolves: one
 * reaching it through some other link is outside it as far as the path alone says. A directory or a FIFO is not read at all, with or without a root.
 */
TEST(SceneImageNodeTest, FileRootRefusesEverythingOutsideIt)
{
	ImageTestRoot tree(_imageTestRedPng());

	ASSERT_TRUE(pngImage::setFileRoot(tree.root));

	const std::string outside[] = {"../outside/secret.png", "sub/../../outside/secret.png",
		tree.outside + "/secret.png", "/etc/passwd", tree.root + "/../outside/secret.png", "../outside/missing.png",
		tree.base + "/rootLink/top.png"};

	for(const std::string& path : outside)
	{
		_expectImageError([&]() { pngImage::fromFile(path); }, ImageException::OUTSIDE_FILE_ROOT, path.c_str());
	}

	const std::string unreadable[] = {"sub/escape.png", "missing.png", "sub", "sub/fifo.png", ""};

	for(const std::string& path : unreadable)
	{
		_expectImageError([&]() { pngImage::fromFile(path); }, ImageException::FILE_UNREADABLE, path.c_str());
	}

	ASSERT_TRUE(pngImage::setFileRoot(""));

	_expectImageError([&]() { pngImage::fromFile(tree.root + "/sub/fifo.png"); }, ImageException::FILE_UNREADABLE,
		"unconfined FIFO");
	_expectImageError([&]() { pngImage::fromFile(tree.root + "/sub"); }, ImageException::FILE_UNREADABLE,
		"unconfined directory");

	EXPECT_NO_THROW(pngImage::fromFile(tree.outside + "/secret.png")) << "Without a root, any file may be read.";
}

/**
 * The root holds for every way a node is made: a hive definition naming a file outside it fails to build, and a
 * builder script naming one is told why.
 */
TEST(SceneImageNodeTest, FileRootHoldsForHivesAndBuilders)
{
	ImageTestRoot tree(_imageTestRedPng());

	ASSERT_TRUE(pngImage::setFileRoot(tree.root));

	JsonHiveLoader inside(R"({ "name": "Inside", "nodes": [
		{ "type": "SceneImageNode", "name": "a", "imageFile": "sub/deep.png" } ] })");

	GraphHive* insideHive = HiveBuilder::build(inside, 2);
	Handle<GraphHive> insideHandle(insideHive);

	EXPECT_TRUE(insideHive -> getNode("a").isValid());

	insideHive -> shutdown();

	JsonHiveLoader escaping(R"({ "name": "Escaping", "nodes": [
		{ "type": "SceneImageNode", "name": "a", "imageFile": "../outside/secret.png" } ] })");

	try
	{
		GraphHive* escapingHive = HiveBuilder::build(escaping, 2);
		escapingHive -> shutdown();
		escapingHive -> decrRef();
		ADD_FAILURE() << "Expected the build to fail";
	}
	catch(PersistException& exception)
	{
		EXPECT_EQ(exception.getError(), PersistException::INVALID_IMAGE);
		EXPECT_NE(exception.getDetail().find("outside the image file root"), std::string::npos)
			<< exception.getDetail();
	}

	BuilderScriptNode* builder = nullptr;

	GraphHive* hive = _buildBuilderHive(R"(
		insideOk = createSceneImageNode{ name = "inside", imageFile = "top.png" }
		escapeOk, escapeReason = createSceneImageNode{ name = "escape", imageFile = "../outside/secret.png" }
		escapeReason = escapeReason or ""
	)", "", builder, false);
	Handle<GraphHive> hiveHandle(hive);

	ASSERT_TRUE(_runBuilderCore(builder));

	EXPECT_TRUE(_coreBool(builder, "insideOk"));
	EXPECT_FALSE(_coreBool(builder, "escapeOk"));
	EXPECT_NE(_coreString(builder, "escapeReason").find("outside the image file root"), std::string::npos);
	EXPECT_FALSE(hive -> getNode("escape").isValid());

	hive -> shutdown();
}

#endif
