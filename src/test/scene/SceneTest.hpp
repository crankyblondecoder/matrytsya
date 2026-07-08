#ifndef SCENE_TEST_H
#define SCENE_TEST_H

#include <gtest/gtest.h>

#include "../../graph/actions/SceneStrobeAction.hpp"
#include "../../graph/GraphHive.hpp"
#include "../../graph/GraphHiveHandle.hpp"
#include "../../graph/GraphHiveSceneSurface.hpp"
#include "../../graph/GraphNodeHandle.hpp"
#include "../../graph/nodes/SceneGeometryNode.hpp"
#include "../../graph/nodes/SceneRootNode.hpp"
#include "../../graph/nodes/SceneTransformNode.hpp"

TEST(SceneTest, GeneratedSceneContainsScriptVertexes)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	// The nodes must _not_ be allocated on the stack because of auto-delete once de-referenced.
	SceneRootNode* root = new SceneRootNode();

	SceneGeometryNode* geometryNode = new SceneGeometryNode(
		"addVertex(Vertex{"
		"	posn = {1, 2, 3},"
		"	colour = {0.1, 0.2, 0.3, 0.4},"
		"	texCoords = {0.5, 0.6},"
		"	normal = {0, 0, 1}"
		"})");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphNodeHandle geometryHandle(geometryNode);
	root -> createEdge(geometryHandle);

	GraphNodeHandle rootHandle(root);
	SceneStrobeAction* strobeAction = new SceneStrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = root -> generateSceneSurface(0);

	std::vector<GraphHiveSceneSurface::Chunk> chunks = surface -> getChunks();

	ASSERT_EQ(chunks.size(), 1u);
	ASSERT_EQ(chunks[0].vertexes.size(), 1u);

	const Vertex& vertex = chunks[0].vertexes[0];

	EXPECT_DOUBLE_EQ(vertex.posn[0], 1);
	EXPECT_DOUBLE_EQ(vertex.posn[1], 2);
	EXPECT_DOUBLE_EQ(vertex.posn[2], 3);

	EXPECT_DOUBLE_EQ(vertex.colour[0], 0.1);
	EXPECT_DOUBLE_EQ(vertex.colour[1], 0.2);
	EXPECT_DOUBLE_EQ(vertex.colour[2], 0.3);
	EXPECT_DOUBLE_EQ(vertex.colour[3], 0.4);

	EXPECT_DOUBLE_EQ(vertex.texCoords[0], 0.5);
	EXPECT_DOUBLE_EQ(vertex.texCoords[1], 0.6);

	EXPECT_DOUBLE_EQ(vertex.normal[0], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[1], 0);
	EXPECT_DOUBLE_EQ(vertex.normal[2], 1);

	delete surface;

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneKeepsVertexesInScriptOrder)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();

	SceneGeometryNode* geometryNode = new SceneGeometryNode(
		"addVertexes({"
		"	Vertex{posn = {1, 0, 0}},"
		"	Vertex{posn = {0, 1, 0}},"
		"	Vertex{posn = {0, 0, 1}}"
		"})");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphNodeHandle geometryHandle(geometryNode);
	root -> createEdge(geometryHandle);

	GraphNodeHandle rootHandle(root);
	SceneStrobeAction* strobeAction = new SceneStrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = root -> generateSceneSurface(0);

	std::vector<GraphHiveSceneSurface::Chunk> chunks = surface -> getChunks();

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

	delete surface;

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneUsesIdentityTransformWhenNoneApplied)
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneGeometryNode* geometryNode = new SceneGeometryNode("addVertex(Vertex{posn = {1, 2, 3}})");

	hive -> addNode(root);
	hive -> addNode(geometryNode);

	GraphNodeHandle geometryHandle(geometryNode);
	root -> createEdge(geometryHandle);

	GraphNodeHandle rootHandle(root);
	SceneStrobeAction* strobeAction = new SceneStrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = root -> generateSceneSurface(0);

	std::vector<GraphHiveSceneSurface::Chunk> chunks = surface -> getChunks();
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = surface -> getModelTransforms();

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

	delete surface;

	hive -> shutdown();
}

TEST(SceneTest, GeneratedSceneCombinesNestedTransformsInTraversalOrder)
{
	// Root -> scale (applied first, further from the geometry) -> translate (applied second, closer to the
	// geometry) -> geometry. Scale then translate gives an easily hand-checkable combined transform: the
	// scale factor stays on the diagonal, and the translation column carries the translate node's own offset.

	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	SceneTransformNode* scaleNode = new SceneTransformNode();
	SceneTransformNode* translateNode = new SceneTransformNode();
	SceneGeometryNode* geometryNode = new SceneGeometryNode("addVertex(Vertex{posn = {1, 2, 3}})");

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

	GraphNodeHandle scaleHandle(scaleNode);
	GraphNodeHandle translateHandle(translateNode);
	GraphNodeHandle geometryHandle(geometryNode);

	root -> createEdge(scaleHandle);
	scaleNode -> createEdge(translateHandle);
	translateNode -> createEdge(geometryHandle);

	GraphNodeHandle rootHandle(root);
	SceneStrobeAction* strobeAction = new SceneStrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

	GraphHiveSceneSurface* surface = root -> generateSceneSurface(0);

	std::vector<GraphHiveSceneSurface::Chunk> chunks = surface -> getChunks();
	std::vector<GraphHiveSceneSurface::ModelTransform> modelTransforms = surface -> getModelTransforms();

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

	delete surface;

	hive -> shutdown();
}

#endif
