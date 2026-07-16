#include "graph/GraphHandle.hpp"
#include "graph/GraphHive.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/graphSceneElements.hpp"
#include "graph/nodes/SceneGeometryNode.hpp"
#include "graph/nodes/SceneGeometryScriptNode.hpp"
#include "graph/nodes/SceneRootNode.hpp"
#include "graph/nodes/SceneTransformScriptNode.hpp"
#include "display/GraphHiveSceneSurfaceWebglMap.hpp"
#include "display/http/HttpServer.hpp"

#include <cmath>
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
	// the cos/sin terms _setRotationZ() would have written. Guarded on getStrobe() so the tilt only advances
	// on strobe actions, not on every action that happens to invoke this node's script, and on getAnimating()
	// so it stays paused until an AnimateAction (emitted by the flower centre's toggling poke script, see
	// _BODY_CLICK_SCRIPT below) has marked this node as animating.
	//
	// Bounces between fully open (angle 0) and fully closed (angle maxTilt) forever rather than clamping at
	// maxTilt, so the direction has to survive between strobes. ScriptNode's core state now persists its
	// globals across every invoke() (see its class comment), so the ordinary global `opening` just keeps its
	// value from the previous strobe; it starts nil (falsy), which reads the same as false, so the first ever
	// strobe closes the flower before it opens.
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

	void _hsvToRgb(double hue, double saturation, double value, double& r, double& g, double& b)
	{
		double chroma = value * saturation;
		double huePrime = hue * 6.0;
		double x = chroma * (1.0 - std::fabs(std::fmod(huePrime, 2.0) - 1.0));
		double m = value - chroma;

		double rp = 0.0, gp = 0.0, bp = 0.0;

		if(huePrime < 1.0)      { rp = chroma; gp = x;      bp = 0.0;    }
		else if(huePrime < 2.0) { rp = x;      gp = chroma; bp = 0.0;    }
		else if(huePrime < 3.0) { rp = 0.0;    gp = chroma; bp = x;      }
		else if(huePrime < 4.0) { rp = 0.0;    gp = x;      bp = chroma; }
		else if(huePrime < 5.0) { rp = x;      gp = 0.0;    bp = chroma; }
		else                    { rp = chroma; gp = 0.0;    bp = x;      }

		r = rp + m;
		g = gp + m;
		b = bp + m;
	}

	/**
	 * A single vertex's worth of data, laid out in the same field order that SceneGeometryNode's and
	 * SceneGeometryScriptNode's addVertexes() expect to unpack from a flat double buffer (colour and
	 * texCoords/normal excluded here are filled in by _appendVertex()).
	 */
	struct _RawVertex
	{
		double posn[3];
		double colour[3];
		double texCoords[2];
		double normal[3];
	};

	// Appends one vertex's worth of raw serial data (position, colour + fixed opaque alpha, texture
	// coordinates, normal) to a flat buffer suitable for addVertexes(double*, unsigned) on either
	// SceneGeometryNode or SceneGeometryScriptNode.
	void _appendVertex(std::vector<double>& data, const _RawVertex& vertex)
	{
		data.insert(data.end(), {

			vertex.posn[0], vertex.posn[1], vertex.posn[2],
			vertex.colour[0], vertex.colour[1], vertex.colour[2], 255.0,
			vertex.texCoords[0], vertex.texCoords[1],
			vertex.normal[0], vertex.normal[1], vertex.normal[2]
		});
	}

	// Builds the raw vertex data for a single petal laid out flat along local +X, base at the origin,
	// tapering to a point at both the base and the tip. Colour is interpolated along the petal's length from
	// baseColour (attachment point) to tipColour, giving the gradient; shape is identical for every petal,
	// only the colours differ per call.
	std::vector<double> _buildPetalVertexes(double baseR, double baseG, double baseB, double tipR, double tipG, double tipB)
	{
		const double length = 2.2;
		const double maxHalfWidth = 0.45;
		const int segments = 10;

		std::vector<double> vertexes;

		_RawVertex prevLeft{}, prevRight{};

		for(int i = 0; i <= segments; i++)
		{
			double t = static_cast<double>(i) / segments;
			double x = t * length;
			double halfWidth = maxHalfWidth * std::sin((_TWO_PI / 2.0) * t);

			double colourR = (baseR + (tipR - baseR) * t) * 255.0;
			double colourG = (baseG + (tipG - baseG) * t) * 255.0;
			double colourB = (baseB + (tipB - baseB) * t) * 255.0;

			_RawVertex left{ {x, 0.0, halfWidth}, {colourR, colourG, colourB}, {t, 0.0}, {0.0, 1.0, 0.0} };
			_RawVertex right{ {x, 0.0, -halfWidth}, {colourR, colourG, colourB}, {t, 1.0}, {0.0, 1.0, 0.0} };

			if(i > 0)
			{
				_appendVertex(vertexes, prevLeft);
				_appendVertex(vertexes, prevRight);
				_appendVertex(vertexes, right);

				_appendVertex(vertexes, prevLeft);
				_appendVertex(vertexes, right);
				_appendVertex(vertexes, left);
			}

			prevLeft = left;
			prevRight = right;
		}

		return vertexes;
	}

	// One latitude ring of the half-sphere body, from the equator (latIndex 0) up to the pole (latIndex ==
	// latitudeSegments). Colour is interpolated by latitude, from rimColour at the equator to apexColour at
	// the pole.
	std::vector<_RawVertex> _sphereRing(int latIndex, int latitudeSegments, int longitudeSegments, double radius,
		double rimR, double rimG, double rimB, double apexR, double apexG, double apexB)
	{
		double latFraction = static_cast<double>(latIndex) / latitudeSegments;
		double phi = latFraction * (_TWO_PI / 4.0);
		double y = radius * std::sin(phi);
		double r = radius * std::cos(phi);

		double colourR = (rimR + (apexR - rimR) * latFraction) * 255.0;
		double colourG = (rimG + (apexG - rimG) * latFraction) * 255.0;
		double colourB = (rimB + (apexB - rimB) * latFraction) * 255.0;

		std::vector<_RawVertex> ringVertexes;
		ringVertexes.reserve(longitudeSegments + 1);

		for(int lonIndex = 0; lonIndex <= longitudeSegments; lonIndex++)
		{
			double theta = (static_cast<double>(lonIndex) / longitudeSegments) * _TWO_PI;
			double x = r * std::cos(theta);
			double z = r * std::sin(theta);

			ringVertexes.push_back(_RawVertex{

				{x, y, z},
				{colourR, colourG, colourB},
				{static_cast<double>(lonIndex) / longitudeSegments, latFraction},
				{x / radius, y / radius, z / radius}
			});
		}

		return ringVertexes;
	}

	// Builds the raw vertex data for a half-sphere dome, as a latitude/longitude grid from the equator up to
	// the pole, plus a flat base cap closing the underside.
	std::vector<double> _buildHalfSphereBodyVertexes()
	{
		const double apexR = 1.0, apexG = 0.85, apexB = 0.2;
		const double rimR = 0.3, rimG = 0.5, rimB = 0.15;
		const int latitudeSegments = 10;
		const int longitudeSegments = 20;

		std::vector<double> vertexes;

		auto ring = [&](int latIndex)
		{
			return _sphereRing(latIndex, latitudeSegments, longitudeSegments, _BODY_RADIUS,
				rimR, rimG, rimB, apexR, apexG, apexB);
		};

		std::vector<_RawVertex> prevRing = ring(0);

		for(int latIndex = 1; latIndex <= latitudeSegments; latIndex++)
		{
			std::vector<_RawVertex> currRing = ring(latIndex);

			for(int lonIndex = 0; lonIndex < longitudeSegments; lonIndex++)
			{
				const _RawVertex& bottomLeft = prevRing[lonIndex];
				const _RawVertex& bottomRight = prevRing[lonIndex + 1];
				const _RawVertex& topLeft = currRing[lonIndex];
				const _RawVertex& topRight = currRing[lonIndex + 1];

				_appendVertex(vertexes, bottomLeft);
				_appendVertex(vertexes, bottomRight);
				_appendVertex(vertexes, topRight);

				_appendVertex(vertexes, bottomLeft);
				_appendVertex(vertexes, topRight);
				_appendVertex(vertexes, topLeft);
			}

			prevRing = currRing;
		}

		// Flat base cap, closing off the underside of the dome at the equator.
		std::vector<_RawVertex> baseRing = ring(0);
		_RawVertex centre{ {0.0, 0.0, 0.0}, {rimR * 255.0, rimG * 255.0, rimB * 255.0}, {0.5, 0.5}, {0.0, -1.0, 0.0} };

		for(int lonIndex = 0; lonIndex < longitudeSegments; lonIndex++)
		{
			_RawVertex flatA = baseRing[lonIndex];
			_RawVertex flatB = baseRing[lonIndex + 1];

			flatA.normal[0] = 0.0; flatA.normal[1] = -1.0; flatA.normal[2] = 0.0;
			flatB.normal[0] = 0.0; flatB.normal[1] = -1.0; flatB.normal[2] = 0.0;

			_appendVertex(vertexes, centre);
			_appendVertex(vertexes, flatB);
			_appendVertex(vertexes, flatA);
		}

		return vertexes;
	}

	// Toggles the petal animation on/off on click: the flower centre's poke script flips its own animating
	// flag and emits an AnimateAction that propagates the new flag to every connected AnimateActionTarget
	// downstream (starting with petalTransform0, see _PETAL_CLOSE_SCRIPT), which pauses or resumes the
	// open/close bounce in place without resetting its progress.
	const char* const _BODY_CLICK_SCRIPT = "setAnimating(not getAnimating(), true)";

}

int main(int argc, char const *argv[])
{
	GraphHive* hive = new GraphHive(2);
	GraphHandle<GraphHive> hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	hive -> addNode(root);
	GraphHandle<GraphNode> rootHandle(root);

	SceneGeometryScriptNode* body = new SceneGeometryScriptNode("", _BODY_CLICK_SCRIPT);
	body -> setPokeEnabled(true);
	hive -> addNode(body);
	GraphHandle<GraphNode> bodyHandle(body);

	std::vector<double> bodyVertexes = _buildHalfSphereBodyVertexes();
	body -> addVertexes(bodyVertexes.data(), bodyVertexes.size());

	root -> createEdge(bodyHandle);

	// A GraphNode only ever traverses a single outgoing edge per action (see GraphNode::traverse), so the
	// whole flower has to be laid out as one chain rather than as siblings branching off root: body, then
	// each petal's transform node followed by that petal's geometry node, in turn.
	//
	// Each transform node's local matrix is pre-multiplied onto whatever the chain has accumulated so far
	// (see GraphHiveSceneSurface::addLocalTransform), so a petal's transform only needs to describe the
	// change relative to the previous petal. The first petal is placed directly from the body's identity
	// frame; every petal after that just adds the fixed angular step, since rotations about the same axis
	// compose additively.
	Transform petalPlacement;
	{
		Transform tilt, translateOut;

		_setRotationZ(tilt, _PETAL_TILT_ANGLE_RADIANS);
		_setTranslation(translateOut, _BODY_RADIUS, 0.0, 0.0);
		_multiplyTransforms(petalPlacement, translateOut, tilt);
	}

	Transform petalAngleStep;
	_setRotationY(petalAngleStep, _TWO_PI / _PETAL_COUNT);

	GraphNode* previousNode = body;

	for(int i = 0; i < _PETAL_COUNT; i++)
	{
		double hue = static_cast<double>(i) / _PETAL_COUNT;

		double baseR, baseG, baseB;
		double tipR, tipG, tipB;

		_hsvToRgb(hue, 0.85, 0.95, baseR, baseG, baseB);
		_hsvToRgb(hue, 0.25, 1.0, tipR, tipG, tipB);

		SceneTransformScriptNode* petalTransform = new SceneTransformScriptNode(i == 0 ? _PETAL_CLOSE_SCRIPT : "", "");
		hive -> addNode(petalTransform);
		GraphHandle<GraphNode> petalTransformHandle(petalTransform);

		petalTransform -> setTransform(i == 0 ? petalPlacement : petalAngleStep);

		previousNode -> createEdge(petalTransformHandle);

		SceneGeometryNode* petal = new SceneGeometryNode();
		hive -> addNode(petal);
		GraphHandle<GraphNode> petalHandle(petal);

		std::vector<double> petalVertexes = _buildPetalVertexes(baseR, baseG, baseB, tipR, tipG, tipB);
		petal -> addVertexes(petalVertexes.data(), petalVertexes.size());

		petalTransform -> createEdge(petalHandle);

		previousNode = petal;
	}

	GraphHiveSceneSurface* surface = new GraphHiveSceneSurface(GraphHandle<SceneRootNode>(root), hiveHandle);

	root -> populateSceneSurface(GraphHandle<GraphHiveSceneSurface>(surface));

	HttpServer httpServer(8080);

	GraphHiveSceneSurfaceWebglMap webglMap(httpServer, *surface, "/");

	webglMap.setPollInterval(_WEBGL_POLL_INTERVAL_MS);

	httpServer.start();

	std::cout << "Listening on http://localhost:" << httpServer.getPort() << "/" << std::endl;
	std::cout << "Click the flower centre to toggle the petal open/close animation." << std::endl;

	signal(SIGINT, _handleSigInt);

	// The population loop must not start until the webgl map has served at least one request, or scene
	// population stalls; this is unrelated to the animation itself, which stays a no-op (see
	// _PETAL_CLOSE_SCRIPT's getAnimating() guard) until the flower centre is clicked regardless of when the
	// loop below starts.
	while(_running && !webglMap.hasReceivedFirstRequest())
	{
		webglMap.waitForFirstRequest(500);
	}

	// Strobing is now driven by the hive's own scheduler thread rather than being pumped manually from here;
	// this loop only has to keep the surface populated with the latest scene state.
	hive -> setStrobeEmitter(rootHandle, _STROBE_FREQUENCY_HZ);

	while(_running)
	{
		// webglMap is bound to this surface for its whole lifetime; it picks up the refreshed contents via the
		// surface changed event fired by populateEnd().
		root -> populateSceneSurface(GraphHandle<GraphHiveSceneSurface>(surface));

		usleep(_STROBE_INTERVAL_US);
	}

	hive -> clearStrobeEmitter(rootHandle);

	httpServer.stop();

	surface -> close();

	hive -> shutdown();

	return 0;
}
