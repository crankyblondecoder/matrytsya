#include "graph/GraphHive.hpp"
#include "graph/GraphHiveHandle.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphNodeHandle.hpp"
#include "graph/actions/SceneStrobeAction.hpp"
#include "graph/nodes/SceneGeometryNode.hpp"
#include "graph/nodes/SceneRootNode.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"

#include <iostream>

#include <signal.h>
#include <unistd.h>

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}

	// Four faces of a regular tetrahedron, built from alternating corners of a cube. Each face is a
	// separate flat-shaded triangle (no shared indexing). Run as a script against a SceneGeometryNode,
	// triggered by a SceneStrobeAction, rather than built directly against the node in C++.
	const char* const TETRAHEDRON_SCRIPT = R"LUA(
local norm = 0.5773502691896258

local faces = {
	{
		normal = { norm, norm, -norm },
		vertexes = {
			{ posn = {  0.5,  0.5,  0.5 }, colour = { 1.0, 0.0, 0.0, 1.0 } },
			{ posn = {  0.5, -0.5, -0.5 }, colour = { 0.0, 1.0, 0.0, 1.0 } },
			{ posn = { -0.5,  0.5, -0.5 }, colour = { 0.0, 0.0, 1.0, 1.0 } },
		}
	},
	{
		normal = { norm, -norm, norm },
		vertexes = {
			{ posn = {  0.5,  0.5,  0.5 }, colour = { 1.0, 0.0, 0.0, 1.0 } },
			{ posn = { -0.5, -0.5,  0.5 }, colour = { 1.0, 1.0, 0.0, 1.0 } },
			{ posn = {  0.5, -0.5, -0.5 }, colour = { 0.0, 1.0, 0.0, 1.0 } },
		}
	},
	{
		normal = { -norm, norm, norm },
		vertexes = {
			{ posn = {  0.5,  0.5,  0.5 }, colour = { 1.0, 0.0, 0.0, 1.0 } },
			{ posn = { -0.5,  0.5, -0.5 }, colour = { 0.0, 0.0, 1.0, 1.0 } },
			{ posn = { -0.5, -0.5,  0.5 }, colour = { 1.0, 1.0, 0.0, 1.0 } },
		}
	},
	{
		normal = { -norm, -norm, -norm },
		vertexes = {
			{ posn = {  0.5, -0.5, -0.5 }, colour = { 0.0, 1.0, 0.0, 1.0 } },
			{ posn = { -0.5, -0.5,  0.5 }, colour = { 1.0, 1.0, 0.0, 1.0 } },
			{ posn = { -0.5,  0.5, -0.5 }, colour = { 0.0, 0.0, 1.0, 1.0 } },
		}
	},
}

local vertexes = {}

for _, face in ipairs(faces) do
	for _, vertex in ipairs(face.vertexes) do
		vertex.normal = face.normal
		vertex.texCoords = { 0.0, 0.0 }

		vertexes[#vertexes + 1] = Vertex(vertex)
	end
end

addVertexes(vertexes)
)LUA";
}

int main(int argc, char const *argv[])
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	hive -> addNode(root);
	GraphNodeHandle rootHandle(root);

	SceneGeometryNode* tetrahedron = new SceneGeometryNode(TETRAHEDRON_SCRIPT);
	hive -> addNode(tetrahedron);
	GraphNodeHandle tetrahedronHandle(tetrahedron);

	root -> createEdge(tetrahedronHandle);

	// Strobe the scene rooted at root, which runs each visited node's script (here, the tetrahedron's
	// geometry-generating script) rather than having geometry set directly against the node.
	SceneStrobeAction* strobeAction = new SceneStrobeAction(rootHandle);

	strobeAction -> incrRef();
	strobeAction -> start();
	strobeAction -> waitOnComplete(0);
	strobeAction -> decrRef();

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
