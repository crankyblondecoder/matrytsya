#ifndef SCENE_TEST_H
#define SCENE_TEST_H

#include <gtest/gtest.h>

#include "../../graph/actions/SceneAction.hpp"
#include "../../graph/actions/StrobeAction.hpp"
#include "../../graph/GraphHandle.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphHiveSceneSurface.hpp"
#include "../../graph/nodes/SceneGeometryScriptNode.hpp"
#include "../../graph/nodes/SceneRootNode.hpp"
#include "../../graph/nodes/SceneTransformScriptNode.hpp"

namespace
{
	/**
	 * Scene surface that records how many populate passes were actually started against it, so a test can
	 * distinguish a SceneAction that (re)populated the surface from one that skipped population because the
	 * scene version was unchanged.
	 */
	class CountingSceneSurface : public GraphHiveSceneSurface
	{
		public:

			CountingSceneSurface(GraphHandle<SceneRootNode> sceneRootNode) : GraphHiveSceneSurface(sceneRootNode) {}

			unsigned getPopulateStartCount()
			{
				return _populateStartCount;
			}

		protected:

			virtual ~CountingSceneSurface(){}

			virtual void _populateStart() override
			{
				_populateStartCount++;

				GraphHiveSceneSurface::_populateStart();
			}

		private:

			// Disable copying.
			CountingSceneSurface(const CountingSceneSurface& copyFrom);
			CountingSceneSurface& operator= (const CountingSceneSurface& copyFrom);

			/// Number of populate passes that have been started on this surface.
			unsigned _populateStartCount = 0;
	};
}

TEST(SceneTest, GeneratedSceneContainsScriptVertexes)
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	SceneRootNode* root = new SceneRootNode();

	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertex(Vertex{"
		"	posn = {1, 2, 3},"
		"	colour = {10, 20, 30, 40},"
		"	texCoords = {0.5, 0.6},"
		"	normal = {0, 0, 1}"
		"})", "");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> geometryHandle(geometryNode);
	root -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);
	sceneAction -> decrRef();

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);

	const Vertex& vertex = chunks[0].vertexes[0];

	EXPECT_DOUBLE_EQ(vertex.posn[0], 1);
	EXPECT_DOUBLE_EQ(vertex.posn[1], 2);
	EXPECT_DOUBLE_EQ(vertex.posn[2], 3);

	EXPECT_EQ(std::to_integer<int>(vertex.colour[0]), 10);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[1]), 20);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[2]), 30);
	EXPECT_EQ(std::to_integer<int>(vertex.colour[3]), 40);

	EXPECT_DOUBLE_EQ(vertex.texCoords[0], 0.5);
	EXPECT_DOUBLE_EQ(vertex.texCoords[1], 0.6);

	EXPECT_DOUBLE_EQ(vertex.normal[0], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[1], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[2], 1);

	surface -> close();

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneKeepsVertexesInScriptOrder)
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode(
		"addVertexes({"
		"	Vertex{posn = {1, 0, 0}},"
		"	Vertex{posn = {0, 1, 0}},"
		"	Vertex{posn = {0, 0, 1}}"
		"})", "");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> geometryHandle(geometryNode);
	root -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);
	sceneAction -> decrRef();

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 3u);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[0], 1);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[1], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[2], 0);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[0], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[1], 1);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[1].posn[2], 0);

	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[0], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[1], 0);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[2].posn[2], 1);

	surface -> close();

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneUsesIdentityTransformWhenNoneApplied)
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode("addVertex(Vertex{posn = {1, 2, 3}})", "");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> geometryHandle(geometryNode);
	root -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);
	sceneAction -> decrRef();

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = scene.modelTransforms;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(modelTransforms.size(), 1u) << "No transform node was traversed, so only the initial identity should exist.";

	const Transform& transform = modelTransforms[chunks[0].modelTransformIndex].transform;

	for(int col = 0; col < 4; col++)
	{
		for(int row = 0; row < 4; row++)
		{
			EXPECT_DOUBLE_EQ(transform[col * 4 + row], (col == row) ? 1.0 : 0.0);
		}
	}

	surface -> close();

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneCombinesNestedTransformsInTraversalOrder)
{
	// Root -> scale (applied first, further from the geometry) -> translate (applied second, closer to the
	// geometry) -> geometry. Scale then translate gives an easily hand-checkable combined transform: the
	// scale factor stays on the diagonal, and the translation column carries the translate node's own offset.

	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTransformScriptNode* scaleNode = new SceneTransformScriptNode("", "");
	SceneTransformScriptNode* translateNode = new SceneTransformScriptNode("", "");
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode("addVertex(Vertex{posn = {1, 2, 3}})", "");

	double scaleTransform[16] = {
		2.0, 0.0, 0.0, 0.0,
		0.0, 2.0, 0.0, 0.0,
		0.0, 0.0, 2.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	};

	double translateTransform[16] = {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		3.0, 0.0, 0.0, 1.0
	};

	scaleNode -> setTransform(scaleTransform);
	translateNode -> setTransform(translateTransform);

	hive -> addNode(root);
	hive -> addNode(scaleNode);
	hive -> addNode(translateNode);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> scaleHandle(scaleNode);
	GraphHandle<GraphNode> translateHandle(translateNode);
	GraphHandle<GraphNode> geometryHandle(geometryNode);

	root -> createEdge(scaleHandle, {});
	scaleNode -> createEdge(translateHandle, {});
	translateNode -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);
	sceneAction -> decrRef();

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = scene.modelTransforms;

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);

	// The surface stores raw vertex positions; the model transform is applied downstream, not here.
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[0], 1);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[1], 2);
	EXPECT_DOUBLE_EQ(chunks[0].vertexes[0].posn[2], 3);

	// Identity, then scale, then scale-and-translate combined.
	ASSERT_EQ(modelTransforms.size(), 3u);

	const Transform& combined = modelTransforms[chunks[0].modelTransformIndex].transform;

	double expectedCombined[16] = {
		2.0, 0.0, 0.0, 0.0,
		0.0, 2.0, 0.0, 0.0,
		0.0, 0.0, 2.0, 0.0,
		3.0, 0.0, 0.0, 1.0
	};

	for(int index = 0; index < 16; index++)
	{
		EXPECT_DOUBLE_EQ(combined[index], expectedCombined[index]) << "Mismatch at transform element " << index;
	}

	surface -> close();

	hive -> shutdown();
}

TEST(SceneTest, TransformNodeScriptCanReadAndModifyTransform)
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTransformScriptNode* transformNode = new SceneTransformScriptNode(
		"local t = getTransform();"
		"t[13] = t[13] + 5;"
		"setTransform(t);", "");
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode("addVertex(Vertex{posn = {1, 2, 3}})", "");

	hive -> addNode(root);
	hive -> addNode(transformNode);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> transformHandle(transformNode);
	GraphHandle<GraphNode> geometryHandle(geometryNode);

	root -> createEdge(transformHandle, {});
	transformNode -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	SceneAction* sceneAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	sceneAction -> incrRef();
	sceneAction -> start();
	sceneAction -> waitOnComplete(0);
	sceneAction -> decrRef();

	GraphHiveSceneSurface::Scene scene = surface -> getScene();
	std::vector<GraphHiveSceneSurface::Chunk> chunks = scene.chunks;
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = scene.modelTransforms;

	const Transform& transform = modelTransforms[chunks[0].modelTransformIndex].transform;

	EXPECT_DOUBLE_EQ(transform[12], 5.0) << "Script should have read the identity transform and added 5 to element 13.";

	surface -> close();

	hive -> shutdown();
}

TEST(SceneTest, SurfaceNotRepopulatedWhenSceneUnchanged)
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneGeometryScriptNode* geometryNode = new SceneGeometryScriptNode("addVertex(Vertex{posn = {1, 2, 3}})", "");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphHandle<GraphNode> geometryHandle(geometryNode);
	root -> createEdge(geometryHandle, {});

	GraphHandle<GraphNode> rootHandle(root);
	StrobeAction* strobeAction = new StrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	CountingSceneSurface* surface = new CountingSceneSurface(GraphHandle<SceneRootNode>(root));

	surface -> setHive(hiveHandle);

	// First action: the surface has never been populated, so its populate version differs from the scene
	// version and the surface must be populated.
	SceneAction* firstAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	firstAction -> incrRef();
	firstAction -> start();
	firstAction -> waitOnComplete(0);

	unsigned sceneVersion = firstAction -> getSceneVersion();

	firstAction -> decrRef();

	ASSERT_EQ(surface -> getPopulateStartCount(), 1u) << "The first action must populate the never-before-populated surface.";
	ASSERT_EQ(surface -> getScene().chunks.size(), 1u);
	ASSERT_EQ(surface -> getPopulateVersion(), sceneVersion) << "The populate version should track the scene version just populated.";

	// Second action against the unchanged graph: the computed scene version matches the surface's populate
	// version, so _processNextPass must not start another populate pass.
	SceneAction* secondAction = new SceneAction(rootHandle, GraphHandle<GraphHiveSceneSurface>(surface));

	secondAction -> incrRef();
	secondAction -> start();
	secondAction -> waitOnComplete(0);

	EXPECT_EQ(secondAction -> getSceneVersion(), sceneVersion) << "An unchanged scene must produce the same version.";

	secondAction -> decrRef();

	EXPECT_EQ(surface -> getPopulateStartCount(), 1u) << "An unchanged scene must not trigger a second populate pass.";

	surface -> close();

	hive -> shutdown();
}

#endif
