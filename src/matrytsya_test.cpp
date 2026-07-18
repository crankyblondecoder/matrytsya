#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"
#include "graph/GraphHandle.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphNode.hpp"
#include "graph/graphSceneElements.hpp"
#include "graph/nodes/SceneRootNode.hpp"
#include "persist/HiveBuilder.hpp"
#include "persist/json/JsonHiveLoader.hpp"

#include <cmath>
#include <iostream>
#include <string>

#include <signal.h>
#include <unistd.h>

namespace
{
	volatile sig_atomic_t _running = 1;

	void _handleSigInt(int)
	{
		_running = 0;
	}

	const int _PETAL_COUNT = 7;
	const double _BODY_RADIUS = 0.9;
	const double _PETAL_TILT_ANGLE_RADIANS = 0.45;
	const double _TWO_PI = 2.0 * std::acos(-1.0);

	// A finer-grained, more frequent step reads as smoother motion than a coarser one at the same overall
	// closing duration; the webgl map's poll interval is tightened to match so the browser doesn't skip frames.
	const unsigned _STROBE_INTERVAL_US = 40000;
	const unsigned _STROBE_FREQUENCY_HZ = 1000000 / _STROBE_INTERVAL_US;
	const unsigned _WEBGL_POLL_INTERVAL_MS = 50;

	// Runs against the first petal's transform node only: that node's matrix is the sole place the shared
	// outward tilt lives (see the petalPlacement comment below), so nudging its rotation-Z component here is
	// enough to fold every petal in unison on each strobe, since every other petal's transform chains off it.
	// Elements 1/2/5/6 (Lua 1-based) are transform[0], transform[1], transform[4], transform[5], i.e. exactly
	// the cos/sin terms a rotation-Z matrix would have there. Guarded on getStrobe() so the tilt only advances
	// on strobe actions, not on every action that happens to invoke this node's script, and on getAnimating()
	// so it stays paused until an AnimateAction (emitted by the flower centre's toggling poke script, see
	// _BODY_CLICK_SCRIPT below) has marked this node as animating.
	//
	// Bounces between fully open (angle 0) and fully closed (angle maxTilt) forever rather than clamping at
	// maxTilt, so the direction has to survive between strobes. ScriptNode's core state persists its globals
	// across every invoke() (see its class comment), so the ordinary global `opening` just keeps its value
	// from the previous strobe; it starts nil (falsy), which reads the same as false, so the first ever strobe
	// closes the flower before it opens.
	const char* const _PETAL_CLOSE_SCRIPT =
		"if getStrobe() and getAnimating() then"
		"	local t = getTransform();"
		"	local angle = math.atan(t[2], t[1]);"
		"	local step = 0.0028;"
		"	local maxTilt = 1.5;"
		"	if opening then"
		"		angle = angle - step;"
		"		if angle <= 0 then angle = 0; opening = false; end"
		"	else"
		"		angle = angle + step;"
		"		if angle >= maxTilt then angle = maxTilt; opening = true; end"
		"	end"
		"	local c = math.cos(angle);"
		"	local s = math.sin(angle);"
		"	t[1] = c; t[2] = s; t[5] = -s; t[6] = c;"
		"	setTransform(t);"
		"end";

	// Toggles the petal animation on/off on click: the flower centre's poke script flips its own animating
	// flag and emits an AnimateAction that propagates the new flag to every connected AnimateActionTarget
	// downstream (starting with petalTransform0, see _PETAL_CLOSE_SCRIPT), which pauses or resumes the
	// open/close bounce in place without resetting its progress.
	const char* const _BODY_CLICK_SCRIPT = "setAnimating(not getAnimating(), true)";

	void _setIdentity(Transform transform)
	{
		for(int i = 0; i < 16; i++) transform[i] = (i % 5 == 0) ? 1.0 : 0.0;
	}

	void _setTranslation(Transform transform, double x, double y, double z)
	{
		_setIdentity(transform);

		transform[12] = x;
		transform[13] = y;
		transform[14] = z;
	}

	void _setRotationY(Transform transform, double angle)
	{
		_setIdentity(transform);

		double cosAngle = std::cos(angle);
		double sinAngle = std::sin(angle);

		transform[0] = cosAngle;  transform[2] = -sinAngle;
		transform[8] = sinAngle;  transform[10] = cosAngle;
	}

	void _setRotationZ(Transform transform, double angle)
	{
		_setIdentity(transform);

		double cosAngle = std::cos(angle);
		double sinAngle = std::sin(angle);

		transform[0] = cosAngle;  transform[1] = sinAngle;
		transform[4] = -sinAngle; transform[5] = cosAngle;
	}

	// Column-major multiply, matching GraphHiveSceneSurface's convention: result = a * b.
	void _multiplyTransforms(Transform result, const Transform a, const Transform b)
	{
		for(int col = 0; col < 4; col++)
		{
			for(int row = 0; row < 4; row++)
			{
				double sum = 0.0;

				for(int k = 0; k < 4; k++) sum += a[k * 4 + row] * b[col * 4 + k];

				result[col * 4 + row] = sum;
			}
		}
	}

	// Turns arbitrary text into a JSON string literal (quotes included): escapes the two characters that
	// would otherwise break out of a JSON string, and collapses any run of newlines/tabs down to a single
	// space so a nicely indented, multi-line Lua script (see _bodyVertexScript()/_petalVertexScript() below)
	// still lands as one valid JSON string value. None of the hand-written Lua below actually needs the quote
	// escaping - Vertex{} tables use bare identifier keys throughout - but doing it generically here means a
	// future script doesn't have to remember to avoid quotes.
	std::string _jsonString(const std::string& text)
	{
		std::string result;
		result.reserve(text.size() + 2);
		result += '"';

		for(char c : text)
		{
			switch(c)
			{
				case '"':  result += "\\\""; break;
				case '\\': result += "\\\\"; break;

				case '\n': case '\r': case '\t':

					if(!result.empty() && result.back() != ' ') result += ' ';
					break;

				default: result += c;
			}
		}

		result += '"';
		return result;
	}

	std::string _transformJson(const Transform transform)
	{
		std::string json = "[";

		for(int i = 0; i < 16; i++)
		{
			if(i > 0) json += ",";
			json += std::to_string(transform[i]);
		}

		json += "]";
		return json;
	}

	// Builds the half-sphere body's geometry procedurally, the same latitude/longitude grid plus flat base cap
	// that _buildHalfSphereBodyVertexes() used to build in C++, just run as this node's coreScript instead.
	//
	// Guarded on vertexCount() == 0 because coreScript runs again on every strobe tick this node's invoke()
	// sees (ScriptNode persists its Lua globals across invocations, but addVertex() only ever appends to
	// SceneGeometryScriptNode's vertex list - nothing clears it - so without this guard every strobe would
	// pile another copy of the whole dome onto the last).
	//
	// ringVertex() recomputes a ring point's position/colour/texCoord from scratch on every call, including
	// for the flat base cap, rather than reusing a Vertex built by an earlier ring() call: Vertex is opaque
	// userdata on the Lua side (only Vertex{} constructs one and addVertex()/addVertexes() consume one), so
	// there is no way to read a field back out of an existing Vertex to copy it with a different normal.
	std::string _bodyVertexScript()
	{
		return "if vertexCount() == 0 then\n\tlocal radius = " + std::to_string(_BODY_RADIUS) + R"LUA(
	local latSeg = 10
	local lonSeg = 20
	local apexR, apexG, apexB = 255, 216.75, 51
	local rimR, rimG, rimB = 76.5, 127.5, 38.25

	local function ringVertex(latIndex, lonIndex, nx, ny, nz)
		local latFraction = latIndex / latSeg
		local phi = latFraction * (math.pi / 2)
		local y = radius * math.sin(phi)
		local r = radius * math.cos(phi)
		local cR = rimR + (apexR - rimR) * latFraction
		local cG = rimG + (apexG - rimG) * latFraction
		local cB = rimB + (apexB - rimB) * latFraction
		local theta = (lonIndex / lonSeg) * (2 * math.pi)
		local x = r * math.cos(theta)
		local z = r * math.sin(theta)
		if not nx then nx, ny, nz = x / radius, y / radius, z / radius end
		return Vertex{posn = {x, y, z}, colour = {cR, cG, cB, 255}, texCoords = {lonIndex / lonSeg, latFraction}, normal = {nx, ny, nz}}
	end

	local function ring(latIndex)
		local verts = {}
		for lon = 0, lonSeg do verts[lon + 1] = ringVertex(latIndex, lon) end
		return verts
	end

	local prevRing = ring(0)

	for lat = 1, latSeg do
		local currRing = ring(lat)

		for lon = 1, lonSeg do
			local bottomLeft, bottomRight = prevRing[lon], prevRing[lon + 1]
			local topLeft, topRight = currRing[lon], currRing[lon + 1]

			addVertex(bottomLeft); addVertex(bottomRight); addVertex(topRight)
			addVertex(bottomLeft); addVertex(topRight); addVertex(topLeft)
		end

		prevRing = currRing
	end

	for lon = 0, lonSeg - 1 do
		local centre = Vertex{posn = {0, 0, 0}, colour = {rimR, rimG, rimB, 255}, texCoords = {0.5, 0.5}, normal = {0, -1, 0}}
		local flatA = ringVertex(0, lon, 0, -1, 0)
		local flatB = ringVertex(0, lon + 1, 0, -1, 0)

		addVertex(centre); addVertex(flatB); addVertex(flatA)
	end
end
)LUA";
	}

	// Builds one petal's tapered-strip geometry procedurally, the same shape _buildPetalVertexes() used to
	// build in C++, coloured by a base-to-tip gradient computed from hsvToRgb(hue, ...) (a Lua transcription
	// of the C++ _hsvToRgb() this file used to have) so each of the 7 petal nodes only differs by the single
	// `hue` value spliced in below. See _bodyVertexScript() for why this is guarded on vertexCount() == 0.
	std::string _petalVertexScript(double hue)
	{
		return "if vertexCount() == 0 then\n\tlocal hue = " + std::to_string(hue) + R"LUA(
	local length, maxHalfWidth, segments = 2.2, 0.45, 10

	local function hsvToRgb(h, sat, val)
		local chroma = val * sat
		local huePrime = h * 6
		local x = chroma * (1 - math.abs((huePrime % 2) - 1))
		local m = val - chroma
		local rp, gp, bp

		if huePrime < 1 then rp, gp, bp = chroma, x, 0
		elseif huePrime < 2 then rp, gp, bp = x, chroma, 0
		elseif huePrime < 3 then rp, gp, bp = 0, chroma, x
		elseif huePrime < 4 then rp, gp, bp = 0, x, chroma
		elseif huePrime < 5 then rp, gp, bp = x, 0, chroma
		else rp, gp, bp = chroma, 0, x
		end

		return (rp + m) * 255, (gp + m) * 255, (bp + m) * 255
	end

	local baseR, baseG, baseB = hsvToRgb(hue, 0.85, 0.95)
	local tipR, tipG, tipB = hsvToRgb(hue, 0.25, 1.0)

	local prevLeft, prevRight

	for i = 0, segments do
		local t = i / segments
		local x = t * length
		local halfWidth = maxHalfWidth * math.sin(math.pi * t)
		local cR = baseR + (tipR - baseR) * t
		local cG = baseG + (tipG - baseG) * t
		local cB = baseB + (tipB - baseB) * t

		local left = Vertex{posn = {x, 0, halfWidth}, colour = {cR, cG, cB, 255}, texCoords = {t, 0}, normal = {0, 1, 0}}
		local right = Vertex{posn = {x, 0, -halfWidth}, colour = {cR, cG, cB, 255}, texCoords = {t, 1}, normal = {0, 1, 0}}

		if i > 0 then
			addVertex(prevLeft); addVertex(prevRight); addVertex(right)
			addVertex(prevLeft); addVertex(right); addVertex(left)
		end

		prevLeft, prevRight = left, right
	end
end
)LUA";
	}

	std::string _sceneRootNodeJson(const std::string& name, const std::string& edgeTo)
	{
		return "{\"type\":\"SceneRootNode\",\"name\":" + _jsonString(name) +
			",\"edges\":[{\"toNodeName\":" + _jsonString(edgeTo) + "}]}";
	}

	std::string _sceneGeometryScriptNodeJson(const std::string& name, bool pokeEnabled,
		const std::string& coreScript, const std::string& pokeScript, const std::string& edgeTo)
	{
		std::string json = "{\"type\":\"SceneGeometryScriptNode\",\"name\":" + _jsonString(name) +
			",\"pokeEnabled\":" + (pokeEnabled ? "true" : "false") +
			",\"coreScript\":" + _jsonString(coreScript) +
			",\"pokeScript\":" + _jsonString(pokeScript);

		if(!edgeTo.empty()) json += ",\"edges\":[{\"toNodeName\":" + _jsonString(edgeTo) + "}]";

		json += "}";
		return json;
	}

	std::string _sceneTransformScriptNodeJson(const std::string& name, const std::string& coreScript,
		const std::string& pokeScript, const Transform transform, const std::string& edgeTo)
	{
		return "{\"type\":\"SceneTransformScriptNode\",\"name\":" + _jsonString(name) +
			",\"coreScript\":" + _jsonString(coreScript) +
			",\"pokeScript\":" + _jsonString(pokeScript) +
			",\"transform\":" + _transformJson(transform) +
			",\"edges\":[{\"toNodeName\":" + _jsonString(edgeTo) + "}]}";
	}

	// Builds the flower hive as a single JSON string matching hiveSchema.json: a SceneRootNode chained to the
	// clickable body, then each petal's transform node followed by that petal's geometry node, in turn, plus a
	// strobeEmitters entry registering root so the hive itself drives strobing from the moment it's built.
	// Unlike the vertex-heavy nodes, there's no procedural shortcut available for the structure itself -
	// HiveBuilder needs an explicit node/edge for each part of the flower - so this stays a direct, literal
	// assembly of hiveSchema.json text via string concatenation, no writer object involved.
	//
	// A GraphNode only ever traverses a single outgoing edge per action (see GraphNode::traverse), so the
	// whole flower has to be laid out as one chain rather than as siblings branching off root: body, then
	// each petal's transform node followed by that petal's geometry node, in turn.
	//
	// Each transform node's local matrix is pre-multiplied onto whatever the chain has accumulated so far
	// (see GraphHiveSceneSurface::addLocalTransform), so a petal's transform only needs to describe the
	// change relative to the previous petal. The first petal is placed directly from the body's identity
	// frame; every petal after that just adds the fixed angular step, since rotations about the same axis
	// compose additively.
	std::string _buildHiveJson()
	{
		Transform tilt, translateOut, petalPlacement;

		_setRotationZ(tilt, _PETAL_TILT_ANGLE_RADIANS);
		_setTranslation(translateOut, _BODY_RADIUS, 0.0, 0.0);
		_multiplyTransforms(petalPlacement, translateOut, tilt);

		Transform petalAngleStep;
		_setRotationY(petalAngleStep, _TWO_PI / _PETAL_COUNT);

		std::string nodes = _sceneRootNodeJson("root", "body") + "," +
			_sceneGeometryScriptNodeJson("body", true, _bodyVertexScript(), _BODY_CLICK_SCRIPT, "petalTransform0");

		for(int i = 0; i < _PETAL_COUNT; i++)
		{
			std::string transformName = "petalTransform" + std::to_string(i);
			std::string petalName = "petal" + std::to_string(i);
			std::string nextTransformName = (i + 1 < _PETAL_COUNT) ? ("petalTransform" + std::to_string(i + 1)) : "";

			nodes += "," + _sceneTransformScriptNodeJson(transformName, i == 0 ? _PETAL_CLOSE_SCRIPT : "", "",
				i == 0 ? petalPlacement : petalAngleStep, petalName);

			double hue = static_cast<double>(i) / _PETAL_COUNT;

			nodes += "," + _sceneGeometryScriptNodeJson(petalName, false, _petalVertexScript(hue), "", nextTransformName);
		}

		return "{\"name\":\"Flower\",\"nodes\":[" + nodes + "],\"strobeEmitters\":[{\"node\":\"root\",\"frequencyHz\":" +
			std::to_string(_STROBE_FREQUENCY_HZ) + "}]}";
	}
}

int main(int argc, char const *argv[])
{
	JsonHiveLoader loader(_buildHiveJson());

	GraphHive* hive = HiveBuilder::build(loader, 2);
	GraphHandle<GraphHive> hiveHandle(hive);

	GraphHandle<GraphNode> rootHandle = hive -> getNode("root");
	SceneRootNode* root = dynamic_cast<SceneRootNode*>(rootHandle.getInstance());

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root));

	hive -> addSurface(surface);
	surface -> strobe();

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap webglMap(httpServer, *surface, "/");

	webglMap.setPollInterval(_WEBGL_POLL_INTERVAL_MS);

	httpServer.start();

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << "/" << std::endl;
	std::cout << "Click the flower centre to toggle the petal open/close animation." << std::endl;

	signal(SIGINT, _handleSigInt);

	// The population loop must not start until the webgl map has served at least one request, or scene
	// population stalls; this is unrelated to strobing, which the hive has already been driving on its own
	// scheduler thread since _buildHiveJson()'s strobeEmitters entry was registered at build time, and which
	// stays a no-op (see _PETAL_CLOSE_SCRIPT's getAnimating() guard) until the flower centre is clicked
	// regardless of when the loop below starts.
	while(_running && !webglMap.hasReceivedFirstRequest())
	{
		webglMap.waitForFirstRequest(500);
	}

	while(_running)
	{
		// webglMap is bound to this surface for its whole lifetime; it picks up the refreshed contents via the
		// surface changed event fired by populateEnd().
		surface -> strobe();

		usleep(_STROBE_INTERVAL_US);
	}

	httpServer.stop();

	hive -> shutdown();

	return 0;
}
