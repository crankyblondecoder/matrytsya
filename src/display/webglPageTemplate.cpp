#include "webglPageTemplate.hpp"

// The WebGL viewer page. It fetches this map's data endpoint and renders the returned chunks as triangles,
// applying each chunk's model transform on the client before upload so that only a single camera uniform is
// needed to draw the whole scene. It polls the map's revision endpoint to detect a changed or replaced surface,
// only re-fetching and re-uploading scene data when the revision actually changes.
const char* const webglPageTemplate = R"HTMLPAGE(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>%TITLE%</title>
<style>
	html, body { margin: 0; padding: 0; overflow: hidden; background: #202020; }
	canvas { display: block; width: 100vw; height: 100vh; }
	#status { position: absolute; top: 8px; left: 8px; color: #ddd; font-family: sans-serif; font-size: 13px; }
</style>
</head>
<body>
<div id="status">Loading...</div>
<canvas id="glCanvas"></canvas>
<script>
(function() {
	'use strict';

	var canvas = document.getElementById('glCanvas');
	var status = document.getElementById('status');
	var gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');

	if(!gl)
	{
		status.textContent = 'WebGL is not available in this browser.';
		return;
	}

	// -- Column-major 4x4 matrix helpers (same convention as the server side Transform type) --

	function multiply(a, b)
	{
		var out = new Array(16);

		for(var col = 0; col < 4; col++)
		{
			for(var row = 0; row < 4; row++)
			{
				var sum = 0;

				for(var k = 0; k < 4; k++) sum += a[k * 4 + row] * b[col * 4 + k];

				out[col * 4 + row] = sum;
			}
		}

		return out;
	}

	function perspective(fovy, aspect, near, far)
	{
		var f = 1.0 / Math.tan(fovy / 2);
		var nf = 1 / (near - far);

		return [
			f / aspect, 0, 0, 0,
			0, f, 0, 0,
			0, 0, (far + near) * nf, -1,
			0, 0, 2 * far * near * nf, 0
		];
	}

	function rotateY(a)
	{
		var c = Math.cos(a), s = Math.sin(a);

		return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1];
	}

	function rotateX(a)
	{
		var c = Math.cos(a), s = Math.sin(a);

		return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1];
	}

	function translate(x, y, z)
	{
		return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1];
	}

	function transformPoint(m, x, y, z)
	{
		var v = [x, y, z, 1];
		var out = [0, 0, 0, 0];

		for(var row = 0; row < 4; row++)
		{
			var sum = 0;

			for(var k = 0; k < 4; k++) sum += m[k * 4 + row] * v[k];

			out[row] = sum;
		}

		return out;
	}

	function transformNormal(m, x, y, z)
	{
		var v = [x, y, z];
		var out = [0, 0, 0];

		for(var row = 0; row < 3; row++)
		{
			var sum = 0;

			for(var k = 0; k < 3; k++) sum += m[k * 4 + row] * v[k];

			out[row] = sum;
		}

		var len = Math.sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]) || 1;

		return [out[0] / len, out[1] / len, out[2] / len];
	}

	function transformVec4(m, x, y, z, w)
	{
		var v = [x, y, z, w];
		var out = [0, 0, 0, 0];

		for(var row = 0; row < 4; row++)
		{
			var sum = 0;

			for(var k = 0; k < 4; k++) sum += m[k * 4 + row] * v[k];

			out[row] = sum;
		}

		return out;
	}

	// Standard column-major 4x4 matrix inverse (same a[col * 4 + row] convention as the rest of this file), used
	// to unproject a clicked screen point back into a world space picking ray.
	function invertMat4(a)
	{
		var a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3];
		var a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7];
		var a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11];
		var a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15];

		var b00 = a00 * a11 - a01 * a10;
		var b01 = a00 * a12 - a02 * a10;
		var b02 = a00 * a13 - a03 * a10;
		var b03 = a01 * a12 - a02 * a11;
		var b04 = a01 * a13 - a03 * a11;
		var b05 = a02 * a13 - a03 * a12;
		var b06 = a20 * a31 - a21 * a30;
		var b07 = a20 * a32 - a22 * a30;
		var b08 = a20 * a33 - a23 * a30;
		var b09 = a21 * a32 - a22 * a31;
		var b10 = a21 * a33 - a23 * a31;
		var b11 = a22 * a33 - a23 * a32;

		var det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

		if(!det) return null;

		det = 1.0 / det;

		return [
			(a11 * b11 - a12 * b10 + a13 * b09) * det,
			(a02 * b10 - a01 * b11 - a03 * b09) * det,
			(a31 * b05 - a32 * b04 + a33 * b03) * det,
			(a22 * b04 - a21 * b05 - a23 * b03) * det,
			(a12 * b08 - a10 * b11 - a13 * b07) * det,
			(a00 * b11 - a02 * b08 + a03 * b07) * det,
			(a32 * b02 - a30 * b05 - a33 * b01) * det,
			(a20 * b05 - a22 * b02 + a23 * b01) * det,
			(a10 * b10 - a11 * b08 + a13 * b06) * det,
			(a01 * b08 - a00 * b10 - a03 * b06) * det,
			(a30 * b04 - a31 * b02 + a33 * b00) * det,
			(a21 * b02 - a20 * b04 - a23 * b00) * det,
			(a11 * b07 - a10 * b09 - a12 * b06) * det,
			(a00 * b09 - a01 * b07 + a02 * b06) * det,
			(a31 * b01 - a30 * b03 - a32 * b00) * det,
			(a20 * b03 - a21 * b01 + a22 * b00) * det
		];
	}

	function dot3(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

	function cross3(a, b)
	{
		return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
	}

	function normalize3(v)
	{
		var len = Math.sqrt(dot3(v, v)) || 1;

		return [v[0] / len, v[1] / len, v[2] / len];
	}

	// Standard Moller-Trumbore ray/triangle intersection. Returns the ray distance to the hit, or null if the
	// ray misses the triangle or hits behind its origin.
	function rayTriangleIntersect(origin, dir, v0, v1, v2)
	{
		var EPS = 1e-7;

		var e1 = [v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]];
		var e2 = [v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]];

		var h = cross3(dir, e2);
		var a = dot3(e1, h);

		if(Math.abs(a) < EPS) return null;

		var f = 1 / a;
		var s = [origin[0] - v0[0], origin[1] - v0[1], origin[2] - v0[2]];
		var u = f * dot3(s, h);

		if(u < 0 || u > 1) return null;

		var q = cross3(s, e1);
		var v = f * dot3(dir, q);

		if(v < 0 || u + v > 1) return null;

		var t = f * dot3(e2, q);

		return (t > EPS) ? t : null;
	}

	// -- Camera state, driven by mouse drag (orbit) and wheel (zoom) --

	var camera = { yaw: 0.6, pitch: 0.4, distance: 10, centre: [0, 0, 0] };
	var dragging = false;
	var lastX = 0, lastY = 0;

	// Tweens camera.centre from centreAnimFrom to centreAnimTo over CENTRE_ANIM_MS instead of snapping, so a
	// right-click recentre reads as a smooth pan rather than a jump-cut. Null when no tween is in flight.
	var centreAnimFrom = null;
	var centreAnimTo = null;
	var centreAnimStartTime = 0;
	var CENTRE_ANIM_MS = 400;

	// Only auto-fit the camera to the scene once: it's driven off each load's bounding box, and re-fitting on
	// every subsequent load (e.g. from an in-progress animation) would fight the user's own zoom/orbit input.
	var cameraFitted = false;

	// Vertical field of view (radians) of the perspective camera. Shared between the initial-focus fit (which
	// derives the camera distance that makes a focused node span a requested fraction of the viewport) and the
	// per-frame projection, so the two always agree.
	var CAMERA_FOVY = Math.PI / 4;

	// Raw positions and their owning chunk ids from the most recently loaded scene, plus the view/projection
	// matrix used to draw the most recent frame. Together these are enough to pick a chunk under a clicked pixel.
	var lastPositions = [];
	var lastTriangleChunkIds = [];
	var lastChunkPokeable = {};
	var lastViewProj = null;

	// Tracks whether a mousedown started on the canvas, and where, so mouseup can tell a click (poke) apart
	// from the end of a camera drag.
	var mouseDownOnCanvas = false;
	var mouseDownX = 0, mouseDownY = 0;

	// Right-button drag pans camera.centre within the view plane, leaving yaw/pitch/distance alone.
	// panning tracks an in-flight right drag; rightDragMoved records whether it moved far enough to count
	// as a pan rather than a stationary right-click, so the contextmenu handler can tell a pan apart from
	// a recentre click.
	var panning = false;
	var panLastX = 0, panLastY = 0;
	var rightDownX = 0, rightDownY = 0;
	var rightDragMoved = false;

	// Casts a ray through the given screen point (client coordinates) using the last drawn view/projection
	// matrix and returns { chunkId, point } for the nearest chunk it hits (point is the world-space hit
	// location, needed by the right-click recentre handler below), or null if nothing was hit.
	function pickChunkAt(clientX, clientY)
	{
		if(!lastViewProj || lastTriangleChunkIds.length === 0) return null;

		var inverse = invertMat4(lastViewProj);

		if(!inverse) return null;

		var rect = canvas.getBoundingClientRect();
		var ndcX = ((clientX - rect.left) / rect.width) * 2 - 1;
		var ndcY = 1 - ((clientY - rect.top) / rect.height) * 2;

		var nearPoint = transformVec4(inverse, ndcX, ndcY, -1, 1);
		var farPoint = transformVec4(inverse, ndcX, ndcY, 1, 1);

		var origin = [nearPoint[0] / nearPoint[3], nearPoint[1] / nearPoint[3], nearPoint[2] / nearPoint[3]];
		var far = [farPoint[0] / farPoint[3], farPoint[1] / farPoint[3], farPoint[2] / farPoint[3]];

		var dir = normalize3([far[0] - origin[0], far[1] - origin[1], far[2] - origin[2]]);

		var closestT = Infinity;
		var closestChunkId = null;

		for(var triIndex = 0; triIndex < lastTriangleChunkIds.length; triIndex++)
		{
			var base = triIndex * 9;

			var v0 = [lastPositions[base], lastPositions[base + 1], lastPositions[base + 2]];
			var v1 = [lastPositions[base + 3], lastPositions[base + 4], lastPositions[base + 5]];
			var v2 = [lastPositions[base + 6], lastPositions[base + 7], lastPositions[base + 8]];

			var hitT = rayTriangleIntersect(origin, dir, v0, v1, v2);

			if(hitT !== null && hitT < closestT)
			{
				closestT = hitT;
				closestChunkId = lastTriangleChunkIds[triIndex];
			}
		}

		if(closestChunkId === null) return null;

		return {
			chunkId: closestChunkId,
			point: [origin[0] + dir[0] * closestT, origin[1] + dir[1] * closestT, origin[2] + dir[2] * closestT]
		};
	}

	// Pokes the surface backing this map to say the chunk with the given id was clicked on.
	function pokeChunk(chunkId)
	{
		var pokeUrl = window.location.pathname.replace(/\/+$/, '') + '/poke';

		fetch(pokeUrl + '?chunkId=' + chunkId, { method: 'POST' }).catch(function(err)
		{
			status.textContent = 'Failed to poke scene: ' + err;
		});
	}

	canvas.addEventListener('mousedown', function(e)
	{
		if(e.button === 2)
		{
			// Right button drag-pans; a stationary right-click still falls through to the contextmenu
			// listener below (pick-to-recentre). Cancel any in-flight recentre tween so the two don't
			// fight over camera.centre.
			panning = true;
			rightDragMoved = false;
			panLastX = e.clientX; panLastY = e.clientY;
			rightDownX = e.clientX; rightDownY = e.clientY;
			centreAnimTo = null;
			return;
		}

		// Any other non-left button is ignored; excluding it here stops it being treated as the start of
		// an orbit drag.
		if(e.button !== 0) return;

		dragging = true;
		mouseDownOnCanvas = true;
		lastX = e.clientX; lastY = e.clientY;
		mouseDownX = e.clientX; mouseDownY = e.clientY;
	});
	window.addEventListener('mouseup', function(e)
	{
		dragging = false;
		panning = false;

		if(!mouseDownOnCanvas) return;

		mouseDownOnCanvas = false;

		var moveDist = Math.hypot(e.clientX - mouseDownX, e.clientY - mouseDownY);

		// Real pointing devices (especially trackpads) commonly drift several pixels between mousedown and
		// mouseup even on a deliberate click, so this needs to be forgiving. At the 0.01 rad/px orbit
		// sensitivity below, even this much slop is an imperceptible camera nudge if it does turn out to be
		// the start of a drag.
		if(moveDist < 4)
		{
			var picked = pickChunkAt(e.clientX, e.clientY);

			if(picked !== null && lastChunkPokeable[picked.chunkId])
			{
				console.log('Picked chunk ' + picked.chunkId);
				pokeChunk(picked.chunkId);
			}
		}
	});
	window.addEventListener('mousemove', function(e)
	{
		if(panning)
		{
			var dx = e.clientX - panLastX;
			var dy = e.clientY - panLastY;

			panLastX = e.clientX;
			panLastY = e.clientY;

			// Past the same slop threshold used for the left-button click/drag split, this counts as a pan
			// so the trailing contextmenu event won't also recentre.
			if(Math.hypot(e.clientX - rightDownX, e.clientY - rightDownY) >= 4) rightDragMoved = true;

			// Camera right/up in world space: the inverse of the view rotation applied to the camera-space
			// axes, same construction as the light direction below.
			var invViewRot = multiply(rotateY(-camera.yaw), rotateX(-camera.pitch));
			var worldRight = transformNormal(invViewRot, 1, 0, 0);
			var worldUp = transformNormal(invViewRot, 0, 1, 0);

			// World units per screen pixel at the focus depth, so the grabbed point stays pinned under the
			// cursor as it's dragged. At distance d the frustum half-height is d*tan(fovy/2), spanning
			// canvas.height pixels. The centre moves opposite the cursor so the scene follows it.
			var perPixel = (2 * camera.distance * Math.tan(CAMERA_FOVY / 2)) / canvas.height;

			for(var i = 0; i < 3; i++)
			{
				camera.centre[i] -= worldRight[i] * dx * perPixel;
				camera.centre[i] += worldUp[i] * dy * perPixel;
			}

			return;
		}

		if(!dragging) return;

		camera.yaw += (e.clientX - lastX) * 0.01;
		camera.pitch += (e.clientY - lastY) * 0.01;

		var limit = Math.PI / 2 - 0.05;
		camera.pitch = Math.max(-limit, Math.min(limit, camera.pitch));

		lastX = e.clientX;
		lastY = e.clientY;
	});

	canvas.addEventListener('wheel', function(e)
	{
		camera.distance *= (1 + (e.deltaY > 0 ? 0.1 : -0.1));
		camera.distance = Math.max(0.01, camera.distance);
		e.preventDefault();
	}, { passive: false });

	// Right-clicking a pokeable chunk recentres the orbit on the picked point instead of opening the
	// browser's context menu. Distance/yaw/pitch are left alone so this reads as a recentre, not a jump-cut.
	canvas.addEventListener('contextmenu', function(e)
	{
		e.preventDefault();

		// A right drag pans (handled in mousemove above); only a stationary right-click recentres, so a
		// pan doesn't also snap the centre onto whatever chunk happened to be under the release point.
		if(rightDragMoved) return;

		var picked = pickChunkAt(e.clientX, e.clientY);

		if(picked !== null && lastChunkPokeable[picked.chunkId])
		{
			// Start from wherever the centre currently is, which is the tween's midpoint if a previous
			// recentre is still in flight, so a fast double pick doesn't stutter back to the old target.
			centreAnimFrom = camera.centre.slice();
			centreAnimTo = picked.point;
			centreAnimStartTime = performance.now();
		}
	});

	// -- Shaders --

	var vertexShaderSrc =
		'attribute vec3 aPosition;' +
		'attribute vec4 aColor;' +
		'attribute vec3 aNormal;' +
		'uniform mat4 uViewProj;' +
		'uniform vec3 uLightDir;' +
		'varying vec4 vColor;' +
		'void main() {' +
		'  vec3 lightDir = normalize(uLightDir);' +
		'  float diffuse = max(dot(normalize(aNormal), lightDir), 0.0);' +
		'  float lighting = 0.35 + 0.65 * diffuse;' +
		'  vColor = vec4(aColor.rgb * lighting, aColor.a);' +
		'  gl_Position = uViewProj * vec4(aPosition, 1.0);' +
		'}';

	var fragmentShaderSrc =
		'precision mediump float;' +
		'varying vec4 vColor;' +
		'void main() { gl_FragColor = vColor; }';

	function compileShader(type, src)
	{
		var shader = gl.createShader(type);

		gl.shaderSource(shader, src);
		gl.compileShader(shader);

		if(!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
		{
			throw new Error('Shader compile error: ' + gl.getShaderInfoLog(shader));
		}

		return shader;
	}

	var program = gl.createProgram();

	gl.attachShader(program, compileShader(gl.VERTEX_SHADER, vertexShaderSrc));
	gl.attachShader(program, compileShader(gl.FRAGMENT_SHADER, fragmentShaderSrc));
	gl.linkProgram(program);

	if(!gl.getProgramParameter(program, gl.LINK_STATUS))
	{
		throw new Error('Program link error: ' + gl.getProgramInfoLog(program));
	}

	gl.useProgram(program);

	var aPosition = gl.getAttribLocation(program, 'aPosition');
	var aColor = gl.getAttribLocation(program, 'aColor');
	var aNormal = gl.getAttribLocation(program, 'aNormal');
	var uViewProj = gl.getUniformLocation(program, 'uViewProj');
	var uLightDir = gl.getUniformLocation(program, 'uLightDir');

	// Direction of the headlamp-style light in camera space; rotated into world space each frame
	// below so it stays fixed relative to the camera as the user orbits.
	var cameraLightDir = [0.5, 0.7, 1.0];

	var positionBuffer = gl.createBuffer();
	var colorBuffer = gl.createBuffer();
	var normalBuffer = gl.createBuffer();
	var vertexCount = 0;

	function resize()
	{
		canvas.width = window.innerWidth;
		canvas.height = window.innerHeight;
		gl.viewport(0, 0, canvas.width, canvas.height);
	}

	window.addEventListener('resize', resize);
	resize();

	function loadScene(data)
	{
		var positions = [];
		var colors = [];
		var normals = [];
		var triangleChunkIds = [];
		var chunkPokeable = {};

		var minX = Infinity, minY = Infinity, minZ = Infinity;
		var maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;

		// Bounding box built only from chunks that request the initial focus (the union, if several do), plus the
		// smallest requested viewport fraction among them (smallest => most zoomed out, so all focus content fits).
		var hasFocus = false;
		var focusMinX = Infinity, focusMinY = Infinity, focusMinZ = Infinity;
		var focusMaxX = -Infinity, focusMaxY = -Infinity, focusMaxZ = -Infinity;
		var focusFraction = Infinity;

		for(var c = 0; c < data.chunks.length; c++)
		{
			var chunk = data.chunks[c];
			var modelTransform = data.modelTransforms[chunk.modelTransformIndex].transform;

			chunkPokeable[chunk.id] = !!chunk.pokeable;

			var chunkFocus = !!chunk.initialFocus;

			if(chunkFocus)
			{
				var chunkFraction = (typeof chunk.focusViewportFraction === 'number' && chunk.focusViewportFraction > 0) ?
					chunk.focusViewportFraction : 0.5;

				focusFraction = Math.min(focusFraction, chunkFraction);
			}

			for(var v = 0; v < chunk.vertexes.length; v++)
			{
				var vertex = chunk.vertexes[v];

				var worldPos = transformPoint(modelTransform, vertex.posn[0], vertex.posn[1], vertex.posn[2]);
				var worldNormal = transformNormal(modelTransform, vertex.normal[0], vertex.normal[1], vertex.normal[2]);

				positions.push(worldPos[0], worldPos[1], worldPos[2]);
				colors.push(vertex.colour[0] / 255, vertex.colour[1] / 255, vertex.colour[2] / 255,
					vertex.colour[3] / 255);
				normals.push(worldNormal[0], worldNormal[1], worldNormal[2]);

				// Every third vertex completes another triangle belonging to this chunk.
				if(v % 3 === 2) triangleChunkIds.push(chunk.id);

				minX = Math.min(minX, worldPos[0]); maxX = Math.max(maxX, worldPos[0]);
				minY = Math.min(minY, worldPos[1]); maxY = Math.max(maxY, worldPos[1]);
				minZ = Math.min(minZ, worldPos[2]); maxZ = Math.max(maxZ, worldPos[2]);

				if(chunkFocus)
				{
					hasFocus = true;

					focusMinX = Math.min(focusMinX, worldPos[0]); focusMaxX = Math.max(focusMaxX, worldPos[0]);
					focusMinY = Math.min(focusMinY, worldPos[1]); focusMaxY = Math.max(focusMaxY, worldPos[1]);
					focusMinZ = Math.min(focusMinZ, worldPos[2]); focusMaxZ = Math.max(focusMaxZ, worldPos[2]);
				}
			}
		}

		vertexCount = positions.length / 3;

		lastPositions = positions;
		lastTriangleChunkIds = triangleChunkIds;
		lastChunkPokeable = chunkPokeable;

		gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.STATIC_DRAW);

		if(vertexCount > 0 && !cameraFitted)
		{
			var radius = Math.max(0.001, Math.sqrt(
				Math.pow(maxX - minX, 2) + Math.pow(maxY - minY, 2) + Math.pow(maxZ - minZ, 2)) / 2);

			if(hasFocus)
			{
				camera.centre = [(focusMinX + focusMaxX) / 2, (focusMinY + focusMaxY) / 2,
					(focusMinZ + focusMaxZ) / 2];

				var focusRadius = Math.max(0.001, Math.sqrt(
					Math.pow(focusMaxX - focusMinX, 2) + Math.pow(focusMaxY - focusMinY, 2) +
					Math.pow(focusMaxZ - focusMinZ, 2)) / 2);

				// Choose the distance so the focus node's bounding-sphere diameter spans focusFraction of the
				// viewport height. At distance d the frustum half-height is d*tan(fovy/2); the sphere's radius
				// projects to that half-height when focusRadius = focusFraction * d*tan(fovy/2), so solve for d.
				camera.distance = focusRadius / (focusFraction * Math.tan(CAMERA_FOVY / 2));
			}
			else
			{
				camera.centre = [(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2];

				camera.distance = radius * 2.5;
			}

			cameraFitted = true;
		}

		status.textContent = vertexCount + ' vertexes across ' + data.chunks.length + ' chunk(s). Drag to orbit, scroll to zoom, right-drag to pan, right-click a chunk to centre.';
	}

	function draw()
	{
		if(centreAnimTo !== null)
		{
			var animT = Math.min(1, (performance.now() - centreAnimStartTime) / CENTRE_ANIM_MS);

			// Smoothstep: eases in and out instead of the tween starting/stopping abruptly.
			var eased = animT * animT * (3 - 2 * animT);

			camera.centre = [
				centreAnimFrom[0] + (centreAnimTo[0] - centreAnimFrom[0]) * eased,
				centreAnimFrom[1] + (centreAnimTo[1] - centreAnimFrom[1]) * eased,
				centreAnimFrom[2] + (centreAnimTo[2] - centreAnimFrom[2]) * eased
			];

			if(animT >= 1) centreAnimTo = null;
		}

		gl.clearColor(0.125, 0.125, 0.125, 1);
		gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
		gl.enable(gl.DEPTH_TEST);

		if(vertexCount > 0)
		{
			var view = multiply(translate(0, 0, -camera.distance),
				multiply(rotateX(camera.pitch),
				multiply(rotateY(camera.yaw),
				translate(-camera.centre[0], -camera.centre[1], -camera.centre[2]))));

			var proj = perspective(CAMERA_FOVY, canvas.width / canvas.height, 0.01, camera.distance * 100 + 100);

			var viewProj = multiply(proj, view);

			lastViewProj = viewProj;

			gl.uniformMatrix4fv(uViewProj, false, new Float32Array(viewProj));

			// Rotate the camera-space light direction by the inverse of the view rotation so it
			// tracks the camera's orbit instead of staying fixed in world space.
			var invViewRot = multiply(rotateY(-camera.yaw), rotateX(-camera.pitch));
			var worldLightDir = transformNormal(invViewRot, cameraLightDir[0], cameraLightDir[1], cameraLightDir[2]);

			gl.uniform3f(uLightDir, worldLightDir[0], worldLightDir[1], worldLightDir[2]);

			gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
			gl.vertexAttribPointer(aPosition, 3, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aPosition);

			gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
			gl.vertexAttribPointer(aColor, 4, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aColor);

			gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
			gl.vertexAttribPointer(aNormal, 3, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aNormal);

			gl.drawArrays(gl.TRIANGLES, 0, vertexCount);
		}

		requestAnimationFrame(draw);
	}

	var dataUrl = window.location.pathname.replace(/\/+$/, '') + '/data';
	var revisionUrl = window.location.pathname.replace(/\/+$/, '') + '/revision';
	var lastRevision = null;
	var drawing = false;
	var pollTimer = null;

	// A refused connection means the server has gone away; retrying on the usual interval would
	// just spam the console with failed network requests, so stop polling until the page is reloaded.
	function stopPolling(err)
	{
		if(pollTimer !== null)
		{
			clearInterval(pollTimer);
			pollTimer = null;
		}

		status.textContent = 'Lost connection to server: ' + err;
	}

	function poll()
	{
		fetch(revisionUrl).then(function(res) { return res.json(); }).then(function(revisionData)
		{
			if(revisionData.revision === lastRevision) return null;

			lastRevision = revisionData.revision;

			return fetch(dataUrl).then(function(res) { return res.json(); }).then(function(data)
			{
				loadScene(data);

				if(!drawing)
				{
					drawing = true;
					draw();
				}
			});
		}).catch(function(err)
		{
			stopPolling(err);
		});
	}

	poll();
	pollTimer = setInterval(poll, %POLL_INTERVAL_MS%);
})();
</script>
</body>
</html>
)HTMLPAGE";
