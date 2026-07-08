#include "graph/GraphHive.hpp"
#include "graph/GraphHiveHandle.hpp"
#include "graph/GraphHiveSceneSurface.hpp"
#include "graph/GraphNodeHandle.hpp"
#include "graph/graphSceneElements.hpp"
#include "graph/nodes/SceneGeometryNode.hpp"
#include "graph/nodes/SceneRootNode.hpp"
#include "graph/nodes/SceneTransformNode.hpp"
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
	 * A single vertex's worth of data, laid out in the same field order that SceneGeometryNode::addVertexes()
	 * expects to unpack from a flat double buffer (colour and texCoords/normal excluded here are filled in by
	 * _appendVertex()).
	 */
	struct _RawVertex
	{
		double posn[3];
		double colour[3];
		double texCoords[2];
		double normal[3];
	};

	// Appends one vertex's worth of raw serial data (position, colour + fixed opaque alpha, texture
	// coordinates, normal) to a flat buffer suitable for SceneGeometryNode::addVertexes(double*, unsigned).
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
}

int main(int argc, char const *argv[])
{
	GraphHive* hive = new GraphHive(2);
	GraphHiveHandle hiveHandle(hive);

	SceneRootNode* root = new SceneRootNode();
	hive -> addNode(root);
	GraphNodeHandle rootHandle(root);

	SceneGeometryNode* body = new SceneGeometryNode("");
	hive -> addNode(body);
	GraphNodeHandle bodyHandle(body);

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

		SceneTransformNode* petalTransform = new SceneTransformNode();
		hive -> addNode(petalTransform);
		GraphNodeHandle petalTransformHandle(petalTransform);

		petalTransform -> setTransform(i == 0 ? petalPlacement : petalAngleStep);

		previousNode -> createEdge(petalTransformHandle);

		SceneGeometryNode* petal = new SceneGeometryNode("");
		hive -> addNode(petal);
		GraphNodeHandle petalHandle(petal);

		std::vector<double> petalVertexes = _buildPetalVertexes(baseR, baseG, baseB, tipR, tipG, tipB);
		petal -> addVertexes(petalVertexes.data(), petalVertexes.size());

		petalTransform -> createEdge(petalHandle);

		previousNode = petal;
	}

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
