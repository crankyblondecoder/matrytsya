#include "graph/GraphHive.hpp"
#include "graph/GraphHiveHandle.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphNodeHandle.hpp"
#include "graph/graphSceneElements.hpp"
#include "graph/nodes/SceneGeometryNode.hpp"
#include "graph/nodes/SceneRootNode.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"

#include <iostream>
#include <vector>

#include <signal.h>
#include <unistd.h>

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}
}

int main(int argc, char const *argv[])
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	hive -> addNode(root);
	GraphNodeHandle rootHandle(root);

	SceneGeometryNode* tetrahedron = new SceneGeometryNode("");
	hive -> addNode(tetrahedron);
	GraphNodeHandle tetrahedronHandle(tetrahedron);

	root -> createEdge(tetrahedronHandle);

	// Four faces of a regular tetrahedron, built from alternating corners of a
	// cube. Each face is a separate flat-shaded triangle (no shared indexing).
	const double norm = 0.5773502691896258;

	std::vector<Vertex> vertexes(12);

	// Face 0: top, front-right, back-left.
	vertexes[0].posn[0] =  0.5; vertexes[0].posn[1] =  0.5; vertexes[0].posn[2] =  0.5;
	vertexes[1].posn[0] =  0.5; vertexes[1].posn[1] = -0.5; vertexes[1].posn[2] = -0.5;
	vertexes[2].posn[0] = -0.5; vertexes[2].posn[1] =  0.5; vertexes[2].posn[2] = -0.5;

	// Face 1: top, back-right, front-right.
	vertexes[3].posn[0] =  0.5; vertexes[3].posn[1] =  0.5; vertexes[3].posn[2] =  0.5;
	vertexes[4].posn[0] = -0.5; vertexes[4].posn[1] = -0.5; vertexes[4].posn[2] =  0.5;
	vertexes[5].posn[0] =  0.5; vertexes[5].posn[1] = -0.5; vertexes[5].posn[2] = -0.5;

	// Face 2: top, back-left, back-right.
	vertexes[6].posn[0] =  0.5; vertexes[6].posn[1] =  0.5; vertexes[6].posn[2] =  0.5;
	vertexes[7].posn[0] = -0.5; vertexes[7].posn[1] =  0.5; vertexes[7].posn[2] = -0.5;
	vertexes[8].posn[0] = -0.5; vertexes[8].posn[1] = -0.5; vertexes[8].posn[2] =  0.5;

	// Face 3: front-right, back-right, back-left.
	vertexes[9].posn[0]  =  0.5; vertexes[9].posn[1]  = -0.5; vertexes[9].posn[2]  = -0.5;
	vertexes[10].posn[0] = -0.5; vertexes[10].posn[1] = -0.5; vertexes[10].posn[2] =  0.5;
	vertexes[11].posn[0] = -0.5; vertexes[11].posn[1] =  0.5; vertexes[11].posn[2] = -0.5;

	vertexes[0].colour[0] = 1.0; vertexes[0].colour[1] = 0.0; vertexes[0].colour[2] = 0.0; vertexes[0].colour[3] = 1.0;
	vertexes[1].colour[0] = 0.0; vertexes[1].colour[1] = 1.0; vertexes[1].colour[2] = 0.0; vertexes[1].colour[3] = 1.0;
	vertexes[2].colour[0] = 0.0; vertexes[2].colour[1] = 0.0; vertexes[2].colour[2] = 1.0; vertexes[2].colour[3] = 1.0;

	vertexes[3].colour[0] = 1.0; vertexes[3].colour[1] = 0.0; vertexes[3].colour[2] = 0.0; vertexes[3].colour[3] = 1.0;
	vertexes[4].colour[0] = 1.0; vertexes[4].colour[1] = 1.0; vertexes[4].colour[2] = 0.0; vertexes[4].colour[3] = 1.0;
	vertexes[5].colour[0] = 0.0; vertexes[5].colour[1] = 1.0; vertexes[5].colour[2] = 0.0; vertexes[5].colour[3] = 1.0;

	vertexes[6].colour[0] = 1.0; vertexes[6].colour[1] = 0.0; vertexes[6].colour[2] = 0.0; vertexes[6].colour[3] = 1.0;
	vertexes[7].colour[0] = 0.0; vertexes[7].colour[1] = 0.0; vertexes[7].colour[2] = 1.0; vertexes[7].colour[3] = 1.0;
	vertexes[8].colour[0] = 1.0; vertexes[8].colour[1] = 1.0; vertexes[8].colour[2] = 0.0; vertexes[8].colour[3] = 1.0;

	vertexes[9].colour[0]  = 0.0; vertexes[9].colour[1]  = 1.0; vertexes[9].colour[2]  = 0.0; vertexes[9].colour[3]  = 1.0;
	vertexes[10].colour[0] = 1.0; vertexes[10].colour[1] = 1.0; vertexes[10].colour[2] = 0.0; vertexes[10].colour[3] = 1.0;
	vertexes[11].colour[0] = 0.0; vertexes[11].colour[1] = 0.0; vertexes[11].colour[2] = 1.0; vertexes[11].colour[3] = 1.0;

	for(int face = 0; face < 4; ++face)
	{
		double faceNormal[3];

		switch(face)
		{
			case 0: faceNormal[0] =  norm; faceNormal[1] =  norm; faceNormal[2] = -norm; break;
			case 1: faceNormal[0] =  norm; faceNormal[1] = -norm; faceNormal[2] =  norm; break;
			case 2: faceNormal[0] = -norm; faceNormal[1] =  norm; faceNormal[2] =  norm; break;
			default: faceNormal[0] = -norm; faceNormal[1] = -norm; faceNormal[2] = -norm; break;
		}

		for(int i = 0; i < 3; ++i)
		{
			Vertex& vertex = vertexes[face * 3 + i];

			vertex.normal[0] = faceNormal[0];
			vertex.normal[1] = faceNormal[1];
			vertex.normal[2] = faceNormal[2];

			vertex.texCoords[0] = 0.0;
			vertex.texCoords[1] = 0.0;
		}
	}

	tetrahedron -> addVertexes(vertexes);

	GraphHiveSceneSurface* surface = root -> generateSceneSurface(0);

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap webglMap(httpServer, *surface, "/");

	httpServer.start();

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << "/" << std::endl;

	signal(SIGINT, _handleSigInt);

	while(_running)
	{
		pause();
	}

	httpServer.stop();

	delete surface;

	hive -> shutdown();

	return 0;
}
