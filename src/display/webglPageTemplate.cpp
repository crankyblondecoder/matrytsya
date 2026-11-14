#include "webglPageTemplate.hpp"

// The WebGL viewer page. It fetches this map's data endpoint and renders the returned chunks as triangles,
// applying each chunk's model transform on the client before upload so that only a single camera uniform is
// needed to draw the whole scene. It polls the map's revision endpoint to detect a changed or replaced surface,
// only re-fetching scene data when the revision actually changes. The data fetch itself is incremental: the
// viewer tells the server which chunks it already holds vertex data for (by id and vertex version), the server
// omits vertexes for any chunk that hasn't changed, and the viewer reuses its cached per-chunk geometry -
// re-deriving world-space positions/normals only for chunks whose model transform actually changed - instead of
// reprocessing the whole scene on every poll.
// The chat window this page carries is not part of the viewer and is not written here: the page only says where
// its style, markup and script go, and the map filling this template in puts them there (see chatWindowTemplate).
const char* const webglPageTemplate = R"HTMLPAGE(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>%TITLE%</title>
<style>
	html, body { margin: 0; padding: 0; overflow: hidden; background: #202020; }
	canvas { display: block; width: 100vw; height: 100vh; }
	#status { position: absolute; top: 8px; left: 8px; color: #ddd; font-family: sans-serif; font-size: 13px; }
	#viewers { position: absolute; top: 8px; right: 12px; color: #ddd; font-family: sans-serif; font-size: 13px; }
	#structureLink { position: absolute; right: 12px; bottom: 12px; padding: 6px 12px; color: #ddd;
		background: #303030; border: 1px solid #4a4a4a; border-radius: 4px; font-family: sans-serif;
		font-size: 13px; text-decoration: none; cursor: pointer; }
	#structureLink:hover { background: #3a3a3a; }
	#sceneTexts { position: absolute; top: 0; left: 0; width: 100vw; height: 100vh; overflow: hidden;
		pointer-events: none; }
	.sceneText { position: absolute; top: 0; left: 0; transform: translate(-50%, -50%); white-space: pre;
		line-height: 1; font-family: sans-serif; text-align: center; pointer-events: none;
		text-shadow: 0 0 0.15em #000, 0 0 0.3em #000; }
%CHAT_STYLE%
</style>
</head>
<body>
<canvas id="glCanvas"></canvas>
<div id="sceneTexts"></div>
<div id="status">Loading...</div>
<div id="viewers"></div>
%STRUCTURE_MARKUP%
%CHAT_MARKUP%
<script>
(function() {
	'use strict';

	var canvas = document.getElementById('glCanvas');
	var status = document.getElementById('status');

	// The structure link is only on the page when the server had somewhere to point it, and the chat toggle
	// only when the page carries a chat window, so both are moved out of the way of each other here rather
	// than in the style rules that put them in the same corner. Measured rather than assumed, so the two stay
	// side by side however wide the chat toggle's own label makes it. Done before the WebGL check below, so a
	// browser that cannot draw the scene can still be used to reach the rest of the hive.
	var structureLink = document.getElementById('structureLink');
	var chatToggle = document.getElementById('chatToggle');

	if(structureLink && chatToggle) structureLink.style.right = (chatToggle.offsetWidth + 20) + 'px';

	// Tell the structure page which surface it is being jumped to from, so that it can offer the way back.
	// Added here rather than built into the link by the server, which has no way of knowing whether a page
	// will be arrived at by pressing this or opened on its own.
	if(structureLink)
	{
		structureLink.href = structureLink.getAttribute('href') + '?from=' +
			encodeURIComponent(window.location.pathname);
	}

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

	// Tweens camera.distance alongside the centre tween, sharing its start time and duration, when a text is
	// brought close enough to read. Null when no distance tween is in flight.
	var distanceAnimFrom = 0;
	var distanceAnimTo = null;

	// Only auto-fit the camera to the scene once: it's driven off each load's bounding box, and re-fitting on
	// every subsequent load (e.g. from an in-progress animation) would fight the user's own zoom/orbit input.
	var cameraFitted = false;

	// -- Remembering where the camera was left --

	// Held against this page's own path, so a surface is come back to as it was left rather than as some
	// other surface was. For the browser session only: a hive that has since been rebuilt should not have a
	// camera pointed for ever at where something used to be.
	var CAMERA_STORE_KEY = 'matrytsya.camera.' + window.location.pathname;

	// Whether a value is a number that can be pointed a camera by. Written out rather than asked of isFinite,
	// so that what is checked is plain to read.
	function usableNumber(value)
	{
		return typeof value === 'number' && value === value && value !== Infinity && value !== -Infinity;
	}

	function saveCamera()
	{
		try
		{
			window.sessionStorage.setItem(CAMERA_STORE_KEY, JSON.stringify({

				yaw: camera.yaw,
				pitch: camera.pitch,
				distance: camera.distance,
				centre: camera.centre
			}));
		}
		catch(err)
		{
			// Storage can be switched off, or full. Not being able to remember where the camera was left is
			// no reason to stop drawing.
		}
	}

	// Puts the camera back where this page was last left, and says whether there was anywhere to put it.
	// What is read back was written by this page itself, but it still comes from outside it, so it is checked
	// rather than trusted: a camera set from nonsense shows nothing and gives no clue why.
	function restoreCamera()
	{
		var saved = null;

		try
		{
			saved = JSON.parse(window.sessionStorage.getItem(CAMERA_STORE_KEY));
		}
		catch(err)
		{
			return false;
		}

		if(!saved || !saved.centre || saved.centre.length !== 3) return false;

		if(!usableNumber(saved.yaw) || !usableNumber(saved.pitch) || !usableNumber(saved.distance) ||
			saved.distance <= 0)
		{
			return false;
		}

		for(var axis = 0; axis < 3; axis++) if(!usableNumber(saved.centre[axis])) return false;

		camera.yaw = saved.yaw;
		camera.pitch = saved.pitch;
		camera.distance = saved.distance;
		camera.centre = [saved.centre[0], saved.centre[1], saved.centre[2]];

		return true;
	}

	// Both, because which of the two a browser gives on the way out of a page differs between them and
	// between how the page is left.
	window.addEventListener('beforeunload', saveCamera);
	window.addEventListener('pagehide', saveCamera);

	// A camera put back where it was left is already framed, so whatever this page does to frame one for the
	// first time is left alone.
	if(restoreCamera()) cameraFitted = true;

	// Vertical field of view (radians) of the perspective camera. Shared between the initial-focus fit (which
	// derives the camera distance that makes a focused node span a requested fraction of the viewport) and the
	// per-frame projection, so the two always agree.
	var CAMERA_FOVY = Math.PI / 4;

	// Raw positions and their owning chunk ids from the most recently loaded scene, plus the view/projection
	// matrix used to draw the most recent frame. Together these are enough to pick a chunk under a clicked pixel.
	// Only ALWAYS-visible chunks go in here, so hover-revealed geometry never intercepts a pick.
	var lastPositions = [];
	var lastTriangleChunkIds = [];
	var lastChunkPokeable = {};
	var lastViewProj = null;

	// The most recently loaded scene, split per chunk with world-space geometry precomputed, so that only the
	// currently visible chunks need be concatenated and uploaded whenever the hover state changes. Each entry is
	// { id, nodeId, opaque, agent, positions, colors, normals }; opaque means VertexVisibility.ALWAYS and agent
	// means VertexVisibility.AGENT.
	var sceneChunks = [];

	// Maps a chunk id to the id of the node that owns it, so hovering one of a node's chunks can reveal that
	// same node's HOVERED_OVER chunks.
	var chunkNodeId = {};

	// Per-chunk cache, keyed by nodeId + ':' + chunkId, surviving across loadScene() calls. Holds each chunk's
	// last known vertexVersion plus its local (untransformed) positions/colors/normals and the world-space
	// positions/normals last derived from them, so a load where the server omits a chunk's vertexes (because
	// this cache already reported holding that version) can reuse the local data instead of asking again, and a
	// load where only the chunk's model transform changed can re-derive world-space geometry from the cached
	// local data without needing fresh vertexes at all. Rebuilt (not mutated) on every load so chunks no longer
	// present in the scene are naturally dropped.
	var chunkCache = {};

	// The model transforms from the previous loadScene() call, so each load only needs to redo the per-vertex
	// world-space transform for chunks whose model transform actually changed, rather than for the whole scene.
	var lastModelTransforms = [];

	// Id of the node currently under the pointer (via one of its pokeable chunks), or null. Drives which
	// HOVERED_OVER chunks are shown.
	var hoveredNodeId = null;

	// Set of node ids the server says currently have an agentic action being applied to them, as reported by
	// each load's agentVisibleNodeIds. Drives which AGENT chunks are shown; unlike the other non-ALWAYS
	// visibilities that is server state rather than something the pointer decides here.
	var agentVisibleNodes = {};

	// -- Scene text --

	// Text is drawn as HTML over the canvas rather than as glyphs in the scene, the same way the structure page
	// labels its nodes. It is sized from its world height and its depth, so it grows and shrinks with distance
	// exactly as the geometry around it does, and is always on top of that geometry. Once it is too small to
	// read it is swapped for a marker: a billboard in the scene the shape of the text block, which unlike the
	// text can be hidden behind other geometry.

	var sceneTextLayer = document.getElementById('sceneTexts');

	// Text smaller than this many pixels high is not worth reading, so is swapped for its marker.
	var MIN_TEXT_PX = 8;

	// How tall a line of text is made when it is right-clicked to be read.
	var READABLE_TEXT_PX = 32;

	// Rough width of a character as a fraction of the line height, used to give a marker the footprint of the
	// text it stands in for. The text itself is laid out by the browser, so this only has to be about right.
	var MARKER_CHAR_WIDTH = 0.6;

	// The text from the most recent load, each entry being { nodeId, worldPos, worldHeight, el, shown, depth,
	// marker, markerHalfWidth, markerHalfHeight, markerShape, markerShapeColors, markerPositions }. shown says the
	// HTML text is currently on screen; marker says the text is too small and its marker is being drawn instead.
	// markerShape is the marker's triangles as x, y pairs in the plane of the screen, and markerPositions those
	// triangles as last drawn, as they are turned to face the camera on every draw.
	var sceneTexts = [];

	// The element showing each node's text, kept against the node id so that a load only touches what changed
	// rather than rebuilding every element.
	var sceneTextElements = {};

	// Number of vertices at the front of the uploaded buffers that make up the opaque pass; the remainder are
	// the currently visible translucent (non-ALWAYS) chunks, drawn as a second blended pass.
	var opaqueVertexCount = 0;

	// Id of the pokeable chunk the pointer is currently over, or null. Used to edge-trigger a HOVER poke only
	// when this changes, rather than flooding the server with one per mousemove.
	var lastHoveredChunkId = null;

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
	// matrix, returning { origin, dir } in world space, or null if there is no frame to cast it through yet.
	function rayAt(clientX, clientY)
	{
		if(!lastViewProj) return null;

		var inverse = invertMat4(lastViewProj);

		if(!inverse) return null;

		var rect = canvas.getBoundingClientRect();
		var ndcX = ((clientX - rect.left) / rect.width) * 2 - 1;
		var ndcY = 1 - ((clientY - rect.top) / rect.height) * 2;

		var nearPoint = transformVec4(inverse, ndcX, ndcY, -1, 1);
		var farPoint = transformVec4(inverse, ndcX, ndcY, 1, 1);

		var origin = [nearPoint[0] / nearPoint[3], nearPoint[1] / nearPoint[3], nearPoint[2] / nearPoint[3]];
		var far = [farPoint[0] / farPoint[3], farPoint[1] / farPoint[3], farPoint[2] / farPoint[3]];

		return { origin: origin, dir: normalize3([far[0] - origin[0], far[1] - origin[1], far[2] - origin[2]]) };
	}

	// Nearest ray distance at which the ray hits any of the triangles in a flat position array, or Infinity.
	function nearestTriangleHit(ray, positions)
	{
		var closestT = Infinity;

		for(var base = 0; base + 8 < positions.length; base += 9)
		{
			var hitT = rayTriangleIntersect(ray.origin, ray.dir,
				[positions[base], positions[base + 1], positions[base + 2]],
				[positions[base + 3], positions[base + 4], positions[base + 5]],
				[positions[base + 6], positions[base + 7], positions[base + 8]]);

			if(hitT !== null && hitT < closestT) closestT = hitT;
		}

		return closestT;
	}

	// Returns the on-screen text under the given screen point, or null. Text is drawn over everything else, so
	// where several overlap the nearest is the one taken to be clicked.
	function pickTextAt(clientX, clientY)
	{
		var picked = null;

		for(var index = 0; index < sceneTexts.length; index++)
		{
			var sceneText = sceneTexts[index];

			if(!sceneText.shown) continue;

			var bounds = sceneText.el.getBoundingClientRect();

			if(clientX < bounds.left || clientX > bounds.right || clientY < bounds.top || clientY > bounds.bottom)
			{
				continue;
			}

			if(picked === null || sceneText.depth < picked.depth) picked = sceneText;
		}

		return picked;
	}

	// Casts a ray through the given screen point and returns { sceneText, distance } for the nearest text
	// marker it hits, or null if it hits none.
	function pickMarkerAt(clientX, clientY)
	{
		var ray = rayAt(clientX, clientY);

		if(ray === null) return null;

		var picked = null;

		for(var index = 0; index < sceneTexts.length; index++)
		{
			var sceneText = sceneTexts[index];

			if(!sceneText.marker) continue;

			var hitT = nearestTriangleHit(ray, sceneText.markerPositions);

			if(hitT < Infinity && (picked === null || hitT < picked.distance))
			{
				picked = { sceneText: sceneText, distance: hitT };
			}
		}

		return picked;
	}

	// Casts a ray through the given screen point (client coordinates) using the last drawn view/projection
	// matrix and returns { chunkId, point, distance } for the nearest chunk it hits (point is the world-space
	// hit location, needed by the right-click recentre handler below, and distance is how far along the ray it
	// is), or null if nothing was hit.
	function pickChunkAt(clientX, clientY)
	{
		if(lastTriangleChunkIds.length === 0) return null;

		var ray = rayAt(clientX, clientY);

		if(ray === null) return null;

		var origin = ray.origin;
		var dir = ray.dir;

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
			point: [origin[0] + dir[0] * closestT, origin[1] + dir[1] * closestT, origin[2] + dir[2] * closestT],
			distance: closestT
		};
	}

	// Brings a text to the centre of the view and close enough for a line of it to be READABLE_TEXT_PX high.
	// The camera looks straight at its centre, so the text's depth once there is the camera distance itself.
	function focusText(sceneText)
	{
		var rect = canvas.getBoundingClientRect();

		centreAnimFrom = camera.centre.slice();
		centreAnimTo = sceneText.worldPos.slice();

		distanceAnimFrom = camera.distance;
		distanceAnimTo = Math.max(0.01,
			sceneText.worldHeight * rect.height / (2 * Math.tan(CAMERA_FOVY / 2) * READABLE_TEXT_PX));

		centreAnimStartTime = performance.now();

		scheduleDraw();
	}

	// Pokes the surface backing this map to say the chunk with the given id was interacted with. A chunk id is
	// only unique within its owning node, so the owning node id is sent alongside it to identify the chunk.
	// type is omitted for a click (HIT, the server's default) or passed as 'hoverEnter'/'hoverLeave' when the
	// pointer moves onto or off of a pokeable chunk.
	// Leaves for the page that draws this hive's structure, asking it to point out the node this chunk came
	// from. The link already says where that page is and which surface it is being left from; all this adds
	// is which node to point at.
	function showChunkInStructure(chunkId)
	{
		window.location.href = structureLink.getAttribute('href') + '&node=' + chunkNodeId[chunkId];
	}

	function pokeChunk(chunkId, type)
	{
		var pokeUrl = window.location.pathname.replace(/\/+$/, '') + '/poke';
		var url = pokeUrl + '?nodeId=' + chunkNodeId[chunkId] + '&chunkId=' + chunkId;

		if(type) url += '&type=' + type;

		fetch(url, { method: 'POST' }).catch(function(err)
		{
			status.textContent = 'Failed to poke scene: ' + err;
		});
	}

	// Picks the chunk under the given screen point and, if it's pokeable and different from the last hovered
	// chunk, raises a HOVER_LEAVE poke for the chunk being left and a HOVER_ENTER poke for the chunk being
	// entered. Also flips the cursor to a pointer while over a pokeable chunk, purely as client-side feedback.
	// Edge-triggered on the hovered chunk changing, rather than firing per mousemove, since a naive per-move
	// poke would flood the server with fetches while the pointer just sits still moving a pixel at a time.
	function updateHover(clientX, clientY)
	{
		var picked = pickChunkAt(clientX, clientY);
		var hoveredChunkId = (picked !== null && lastChunkPokeable[picked.chunkId]) ? picked.chunkId : null;

		canvas.style.cursor = (hoveredChunkId !== null) ? 'pointer' : 'default';

		// Reveal (or hide) the hovered node's HOVERED_OVER chunks by rebuilding the buffers when the node under
		// the pointer changes. Keyed on the node rather than the chunk so a node made of several chunks doesn't
		// flicker its halo as the pointer crosses between them.
		var newHoveredNodeId = (hoveredChunkId !== null) ? chunkNodeId[hoveredChunkId] : null;

		if(newHoveredNodeId !== hoveredNodeId)
		{
			hoveredNodeId = newHoveredNodeId;
			rebuildGeometry();
		}

		if(hoveredChunkId === lastHoveredChunkId) return;

		if(lastHoveredChunkId !== null) pokeChunk(lastHoveredChunkId, 'hoverLeave');

		lastHoveredChunkId = hoveredChunkId;

		if(hoveredChunkId !== null) pokeChunk(hoveredChunkId, 'hoverEnter');
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
			distanceAnimTo = null;
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

			if(picked === null) return;

			// Held down, this asks which node in the hive put this here rather than poking it. Offered for
			// anything that can be picked, pokeable or not: what a node is remains worth asking even where
			// there is nothing to be done to it.
			if(e.ctrlKey)
			{
				if(structureLink) showChunkInStructure(picked.chunkId);

				return;
			}

			if(lastChunkPokeable[picked.chunkId])
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

			scheduleDraw();
			return;
		}

		if(!dragging)
		{
			// Only track hover while the camera isn't being manipulated, so an orbit/pan drag doesn't also
			// spam hover pokes for whatever chunk the pointer happens to cross.
			updateHover(e.clientX, e.clientY);
			return;
		}

		camera.yaw += (e.clientX - lastX) * 0.01;
		camera.pitch += (e.clientY - lastY) * 0.01;

		var limit = Math.PI / 2 - 0.05;
		camera.pitch = Math.max(-limit, Math.min(limit, camera.pitch));

		lastX = e.clientX;
		lastY = e.clientY;

		scheduleDraw();
	});

	// Reset hover state when the pointer leaves the canvas entirely, since no further mousemove will arrive
	// to naturally clear it. Raises the matching HOVER_LEAVE poke, since updateHover won't get a chance to.
	canvas.addEventListener('mouseleave', function(e)
	{
		canvas.style.cursor = 'default';

		if(lastHoveredChunkId !== null) pokeChunk(lastHoveredChunkId, 'hoverLeave');

		lastHoveredChunkId = null;

		// Drop any revealed hover geometry now the pointer has left the canvas entirely.
		if(hoveredNodeId !== null)
		{
			hoveredNodeId = null;
			rebuildGeometry();
		}
	});

	canvas.addEventListener('wheel', function(e)
	{
		// The user's own zoom wins over one still being tweened towards a text.
		distanceAnimTo = null;

		camera.distance *= (1 + (e.deltaY > 0 ? 0.1 : -0.1));
		camera.distance = Math.max(0.01, camera.distance);
		e.preventDefault();
		scheduleDraw();
	}, { passive: false });

	// Right-clicking a pokeable chunk recentres the orbit on the picked point instead of opening the
	// browser's context menu. Distance/yaw/pitch are left alone so this reads as a recentre, not a jump-cut.
	canvas.addEventListener('contextmenu', function(e)
	{
		e.preventDefault();

		// A right drag pans (handled in mousemove above); only a stationary right-click recentres, so a
		// pan doesn't also snap the centre onto whatever chunk happened to be under the release point.
		if(rightDragMoved) return;

		// Text is drawn over everything, so it is what was clicked whenever the pointer is on some.
		var pickedText = pickTextAt(e.clientX, e.clientY);

		if(pickedText !== null)
		{
			focusText(pickedText);
			return;
		}

		var picked = pickChunkAt(e.clientX, e.clientY);

		// A marker is part of the scene, so geometry in front of it takes the click instead, pokeable or not.
		var pickedMarker = pickMarkerAt(e.clientX, e.clientY);

		if(pickedMarker !== null && (picked === null || pickedMarker.distance < picked.distance))
		{
			focusText(pickedMarker.sceneText);
			return;
		}

		if(picked !== null && lastChunkPokeable[picked.chunkId])
		{
			// Start from wherever the centre currently is, which is the tween's midpoint if a previous
			// recentre is still in flight, so a fast double pick doesn't stutter back to the old target.
			// A recentre leaves the distance alone, so any zoom towards a text still in flight stops here.
			centreAnimFrom = camera.centre.slice();
			centreAnimTo = picked.point;
			distanceAnimTo = null;
			centreAnimStartTime = performance.now();

			scheduleDraw();
		}
	});

	// -- Shaders --

	// Lighting model chosen by the server: 'BLINN_PHONG' or 'LAMBERT'.
	var SHADING_MODEL = '%SHADING_MODEL%';

	// Lambert: diffuse lighting worked out per vertex and interpolated across each triangle.
	var lambertVertexShaderSrc =
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

	var lambertFragmentShaderSrc =
		'precision mediump float;' +
		'varying vec4 vColor;' +
		'void main() { gl_FragColor = vColor; }';

	// Blinn-Phong: the same ambient/diffuse split as Lambert plus a specular highlight, worked out per fragment
	// since a highlight interpolated from the vertexes of coarse triangles smears or vanishes. Positions and
	// normals arrive already in world space, so they are passed straight through.
	var blinnPhongVertexShaderSrc =
		'attribute vec3 aPosition;' +
		'attribute vec4 aColor;' +
		'attribute vec3 aNormal;' +
		'uniform mat4 uViewProj;' +
		'varying vec4 vColor;' +
		'varying vec3 vNormal;' +
		'varying vec3 vWorldPos;' +
		'void main() {' +
		'  vColor = aColor;' +
		'  vNormal = aNormal;' +
		'  vWorldPos = aPosition;' +
		'  gl_Position = uViewProj * vec4(aPosition, 1.0);' +
		'}';

	var blinnPhongFragmentShaderSrc =
		'precision mediump float;' +
		'uniform vec3 uLightDir;' +
		'uniform vec3 uCameraPos;' +
		'varying vec4 vColor;' +
		'varying vec3 vNormal;' +
		'varying vec3 vWorldPos;' +
		'const float SHININESS = 32.0;' +
		'const float SPECULAR = 0.35;' +
		'void main() {' +
		'  vec3 normal = normalize(vNormal);' +
		'  vec3 lightDir = normalize(uLightDir);' +
		'  vec3 viewDir = normalize(uCameraPos - vWorldPos);' +
		'  vec3 halfDir = normalize(lightDir + viewDir);' +
		'  float diffuse = max(dot(normal, lightDir), 0.0);' +
		// No highlight on a face turned away from the light, which would otherwise catch one at grazing angles.
		'  float specular = diffuse > 0.0 ? pow(max(dot(normal, halfDir), 0.0), SHININESS) : 0.0;' +
		'  vec3 rgb = vColor.rgb * (0.35 + 0.65 * diffuse) + vec3(SPECULAR * specular);' +
		'  gl_FragColor = vec4(rgb, vColor.a);' +
		'}';

	var useBlinnPhong = SHADING_MODEL === 'BLINN_PHONG';
	var vertexShaderSrc = useBlinnPhong ? blinnPhongVertexShaderSrc : lambertVertexShaderSrc;
	var fragmentShaderSrc = useBlinnPhong ? blinnPhongFragmentShaderSrc : lambertFragmentShaderSrc;

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

	// Only the Blinn-Phong program has this, so it is null under Lambert.
	var uCameraPos = gl.getUniformLocation(program, 'uCameraPos');

	// Direction of the headlamp-style light in camera space; rotated into world space each frame
	// below so it stays fixed relative to the camera as the user orbits.
	var cameraLightDir = [0.5, 0.7, 1.0];

	var positionBuffer = gl.createBuffer();
	var colorBuffer = gl.createBuffer();
	var normalBuffer = gl.createBuffer();
	var vertexCount = 0;

	// Text markers are kept in buffers of their own, as they are turned to face the camera on every draw and
	// that must never cost rebuilding the scene's geometry.
	var markerPositionBuffer = gl.createBuffer();
	var markerColorBuffer = gl.createBuffer();
	var markerNormalBuffer = gl.createBuffer();
	var markerVertexCount = 0;

	function bindAttributes(positions, colors, normals)
	{
		gl.bindBuffer(gl.ARRAY_BUFFER, positions);
		gl.vertexAttribPointer(aPosition, 3, gl.FLOAT, false, 0, 0);
		gl.enableVertexAttribArray(aPosition);

		gl.bindBuffer(gl.ARRAY_BUFFER, colors);
		gl.vertexAttribPointer(aColor, 4, gl.FLOAT, false, 0, 0);
		gl.enableVertexAttribArray(aColor);

		gl.bindBuffer(gl.ARRAY_BUFFER, normals);
		gl.vertexAttribPointer(aNormal, 3, gl.FLOAT, false, 0, 0);
		gl.enableVertexAttribArray(aNormal);
	}

	function cssColour(colour)
	{
		return 'rgba(' + colour[0] + ',' + colour[1] + ',' + colour[2] + ',' + (colour[3] / 255) + ')';
	}

	// Styles a text's element with the box behind it. The box's lengths come in the same units as the line
	// height, and the element's font size is the line height on screen, so they are given in em to scale with
	// the text without being touched again as the camera moves.
	function applyTextBackground(el, background, height)
	{
		function em(length) { return (length / height) + 'em'; }

		el.style.backgroundColor = cssColour(background.fillColour);

		// The dark halo only exists to lift text off whatever geometry is behind it, and smudges text sat on a
		// filled box of its own.
		el.style.textShadow = background.fillColour[3] > 0 ? 'none' : '';
		el.style.border = background.borderWidth > 0 ?
			em(background.borderWidth) + ' solid ' + cssColour(background.borderColour) : 'none';
		el.style.padding = em(background.padding);
		el.style.borderRadius = background.cornerRadii.map(em).join(' ');
	}

	// Number of straight segments each rounded corner of a marker is drawn with.
	var MARKER_CORNER_SEGMENTS = 6;

	// Outline of a rounded rectangle centred on the origin, as MARKER_CORNER_SEGMENTS + 1 points per corner
	// running counter-clockwise from the bottom right. radii are in TextBackground's corner order (top left,
	// top right, bottom right, bottom left), and a zero radius repeats its corner point, so that any two
	// outlines line up point for point.
	function roundedOutline(halfWidth, halfHeight, radii)
	{
		// Each corner as the TextBackground index of its radius, its signs along x and y, and the angle its arc
		// starts from.
		var corners = [
			{ radius: radii[2], sx: 1, sy: -1, start: -Math.PI / 2 },
			{ radius: radii[1], sx: 1, sy: 1, start: 0 },
			{ radius: radii[0], sx: -1, sy: 1, start: Math.PI / 2 },
			{ radius: radii[3], sx: -1, sy: -1, start: Math.PI }
		];

		var outline = [];

		for(var c = 0; c < corners.length; c++)
		{
			var corner = corners[c];
			var radius = corner.radius;
			var cx = corner.sx * (halfWidth - radius);
			var cy = corner.sy * (halfHeight - radius);

			for(var s = 0; s <= MARKER_CORNER_SEGMENTS; s++)
			{
				var angle = corner.start + (Math.PI / 2) * s / MARKER_CORNER_SEGMENTS;

				outline.push([cx + radius * Math.cos(angle), cy + radius * Math.sin(angle)]);
			}
		}

		return outline;
	}

	// Works out the marker that stands in for a text once it is too small to read: a flat rounded rectangle
	// with the footprint of the text block and its background box, centred on the text's anchor, filled and
	// bordered the way the box is. Only its shape is built here, as triangles in the plane of the screen; it is
	// turned to face the camera on every draw, the same way the HTML text always faces the screen, and so never
	// shows edge on. It is scaled the same way the HTML text is, by the text's world height.
	function buildMarker(sceneText, text, height, colour, background)
	{
		var lines = text.split('\n');
		var longestLine = 1;

		for(var line = 0; line < lines.length; line++) longestLine = Math.max(longestLine, lines[line].length);

		var scale = height > 0 ? sceneText.worldHeight / height : 0;
		var border = background.borderWidth * scale;
		var boxMargin = background.padding * scale + border;

		var halfWidth = longestLine * MARKER_CHAR_WIDTH * sceneText.worldHeight / 2 + boxMargin;
		var halfHeight = lines.length * sceneText.worldHeight / 2 + boxMargin;

		sceneText.markerHalfWidth = halfWidth;
		sceneText.markerHalfHeight = halfHeight;

		// Radii too big for the box are cut back to fit it; the browser shrinks them all in proportion
		// instead, which only differs where adjacent radii overlap, too small a difference to see on a marker.
		var maxRadius = Math.min(halfWidth, halfHeight);
		var outerRadii = background.cornerRadii.map(function(r) { return Math.min(r * scale, maxRadius); });

		// The inside edge of a border is rounded by what is left of the corner's radius past the border, the
		// same way the browser draws it.
		var showBorder = border > 0 && background.borderColour[3] > 0;
		var innerBorder = showBorder ? Math.min(border, maxRadius) : 0;
		var innerRadii = outerRadii.map(function(r) { return Math.max(r - innerBorder, 0); });

		var outer = roundedOutline(halfWidth, halfHeight, outerRadii);
		var inner = roundedOutline(halfWidth - innerBorder, halfHeight - innerBorder, innerRadii);

		// From afar the text inside a box with no fill reads as a smudge of the text's colour.
		var fill = background.fillColour[3] > 0 ? background.fillColour : colour;

		var shape = [];
		var shapeColors = [];

		function addVertex(point, rgba)
		{
			shape.push(point[0], point[1]);
			shapeColors.push(rgba[0] / 255, rgba[1] / 255, rgba[2] / 255, 1);
		}

		// The inside is convex, so is drawn as a fan around the anchor.
		for(var i = 0; i < inner.length; i++)
		{
			var next = (i + 1) % inner.length;

			addVertex([0, 0], fill);
			addVertex(inner[i], fill);
			addVertex(inner[next], fill);
		}

		// The border as a strip between the outlines, which line up point for point.
		if(showBorder)
		{
			for(var b = 0; b < outer.length; b++)
			{
				var bNext = (b + 1) % outer.length;

				addVertex(inner[b], background.borderColour);
				addVertex(outer[b], background.borderColour);
				addVertex(outer[bNext], background.borderColour);

				addVertex(inner[b], background.borderColour);
				addVertex(outer[bNext], background.borderColour);
				addVertex(inner[bNext], background.borderColour);
			}
		}

		sceneText.markerShape = shape;
		sceneText.markerShapeColors = shapeColors;
		sceneText.markerPositions = [];
	}

	// Puts each text where its anchor is on the screen at a size worked out from its depth, which is the same
	// sizing perspective gives the geometry around it. Text too small to read is hidden and its marker drawn
	// instead, laid out along the given world-space camera right and up axes so that it faces the camera.
	function updateSceneTexts(viewProj, cameraRight, cameraUp, cameraBack)
	{
		var rect = canvas.getBoundingClientRect();
		var tanHalfFovy = Math.tan(CAMERA_FOVY / 2);

		var positions = [];
		var colors = [];
		var normals = [];

		for(var index = 0; index < sceneTexts.length; index++)
		{
			var sceneText = sceneTexts[index];
			var wp = sceneText.worldPos;
			var clip = transformVec4(viewProj, wp[0], wp[1], wp[2], 1);

			sceneText.shown = false;
			sceneText.marker = false;

			// Behind the camera, where there is no sensible place on the screen to put it or its marker.
			if(clip[3] > 0)
			{
				var fontPx = sceneText.worldHeight * rect.height / (2 * clip[3] * tanHalfFovy);

				if(fontPx >= MIN_TEXT_PX)
				{
					sceneText.shown = true;
					sceneText.depth = clip[3];

					sceneText.el.style.fontSize = fontPx + 'px';
					sceneText.el.style.left = ((clip[0] / clip[3]) * 0.5 + 0.5) * rect.width + 'px';
					sceneText.el.style.top = (0.5 - (clip[1] / clip[3]) * 0.5) * rect.height + 'px';
				}
				else
				{
					sceneText.marker = true;

					var shape = sceneText.markerShape;
					var shapeColors = sceneText.markerShapeColors;
					var markerPositions = [];

					for(var v = 0; v < shape.length / 2; v++)
					{
						var sx = shape[v * 2];
						var sy = shape[v * 2 + 1];

						markerPositions.push(
							wp[0] + cameraRight[0] * sx + cameraUp[0] * sy,
							wp[1] + cameraRight[1] * sx + cameraUp[1] * sy,
							wp[2] + cameraRight[2] * sx + cameraUp[2] * sy);

						colors.push(shapeColors[v * 4], shapeColors[v * 4 + 1], shapeColors[v * 4 + 2],
							shapeColors[v * 4 + 3]);
						normals.push(cameraBack[0], cameraBack[1], cameraBack[2]);
					}

					sceneText.markerPositions = markerPositions;

					positions.push.apply(positions, markerPositions);
				}
			}

			sceneText.el.style.display = sceneText.shown ? '' : 'none';
		}

		// Nothing to upload when there were no markers last draw and there are none now either.
		if(positions.length === 0 && markerVertexCount === 0) return;

		markerVertexCount = positions.length / 3;

		gl.bindBuffer(gl.ARRAY_BUFFER, markerPositionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.DYNAMIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, markerColorBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.DYNAMIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, markerNormalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.DYNAMIC_DRAW);
	}

	function resize()
	{
		canvas.width = window.innerWidth;
		canvas.height = window.innerHeight;
		gl.viewport(0, 0, canvas.width, canvas.height);
	}

	window.addEventListener('resize', function() { resize(); scheduleDraw(); });
	resize();

	// Applies a model transform to every (x, y, z) position triple in a flat local-space array, returning a new
	// flat world-space array. Kept separate from vertex parsing so a chunk's cached local data can be
	// re-transformed (e.g. because only its model transform changed) without needing fresh vertexes from the
	// server.
	function deriveWorldPositions(localPositions, modelTransform)
	{
		var out = new Array(localPositions.length);

		for(var i = 0; i < localPositions.length; i += 3)
		{
			var worldPos = transformPoint(modelTransform, localPositions[i], localPositions[i + 1], localPositions[i + 2]);

			out[i] = worldPos[0];
			out[i + 1] = worldPos[1];
			out[i + 2] = worldPos[2];
		}

		return out;
	}

	// As deriveWorldPositions(), but for normals (direction vectors, re-normalised after transforming).
	function deriveWorldNormals(localNormals, modelTransform)
	{
		var out = new Array(localNormals.length);

		for(var i = 0; i < localNormals.length; i += 3)
		{
			var worldNormal = transformNormal(modelTransform, localNormals[i], localNormals[i + 1], localNormals[i + 2]);

			out[i] = worldNormal[0];
			out[i + 1] = worldNormal[1];
			out[i + 2] = worldNormal[2];
		}

		return out;
	}

	function loadScene(data)
	{
		sceneChunks = [];
		chunkNodeId = {};

		// Picking and camera framing only ever consider the ALWAYS-visible geometry, so build their arrays
		// straight from the opaque chunks as the scene is parsed.
		var pickPositions = [];
		var pickTriangleChunkIds = [];
		var chunkPokeable = {};

		var minX = Infinity, minY = Infinity, minZ = Infinity;
		var maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;

		// Bounding box built only from chunks belonging to the surface's initial-focus node, if any.
		var hasFocus = false;
		var focusMinX = Infinity, focusMinY = Infinity, focusMinZ = Infinity;
		var focusMaxX = -Infinity, focusMaxY = -Infinity, focusMaxZ = -Infinity;

		var focusChunkIds = {};

		for(var f = 0; f < data.focusChunkIds.length; f++) focusChunkIds[data.focusChunkIds[f]] = true;

		agentVisibleNodes = {};

		if(data.agentVisibleNodeIds)
		{
			for(var a = 0; a < data.agentVisibleNodeIds.length; a++) agentVisibleNodes[data.agentVisibleNodeIds[a]] = true;
		}

		// A model transform is unchanged from the previous load only if the same index held the same 16
		// values. Comparing per-transform here is cheap (there are usually far fewer transforms than vertices)
		// and lets each chunk below skip re-deriving world-space geometry unless its own transform moved.
		var transformChanged = [];

		for(var t = 0; t < data.modelTransforms.length; t++)
		{
			var prevTransform = lastModelTransforms[t];
			var transform = data.modelTransforms[t].transform;
			var changed = !prevTransform;

			for(var i = 0; !changed && i < 16; i++)
			{
				if(transform[i] !== prevTransform[i]) changed = true;
			}

			transformChanged.push(changed);
		}

		// Rebuilt fresh each load (rather than mutating chunkCache in place) so that a chunk no longer present
		// in data.chunks is naturally dropped instead of lingering forever.
		var newChunkCache = {};

		for(var c = 0; c < data.chunks.length; c++)
		{
			var chunk = data.chunks[c];
			var modelTransform = data.modelTransforms[chunk.modelTransformIndex].transform;
			var cacheKey = chunk.nodeId + ':' + chunk.id;
			var cached = chunkCache[cacheKey];

			// A missing/unknown visibility is treated as ALWAYS, matching the server default.
			var opaque = (chunk.visibility === undefined || chunk.visibility === 'ALWAYS');
			var agentChunk = (chunk.visibility === 'AGENT');

			chunkPokeable[chunk.id] = !!chunk.pokeable;
			chunkNodeId[chunk.id] = chunk.nodeId;

			var chunkFocus = !!focusChunkIds[chunk.id];

			// Local (untransformed) vertex data plus this load's world-space positions/normals derived from it.
			// Colors never need transforming, so they're identical whichever branch below fills them in.
			var localPositions, colors, localNormals, worldPositions, worldNormals;

			if(chunk.vertexes !== null)
			{
				// Fresh vertex data from the server: parse into local arrays, then derive world-space geometry.
				localPositions = [];
				colors = [];
				localNormals = [];

				for(var v = 0; v < chunk.vertexes.length; v++)
				{
					var vertex = chunk.vertexes[v];

					localPositions.push(vertex.posn[0], vertex.posn[1], vertex.posn[2]);
					colors.push(vertex.colour[0] / 255, vertex.colour[1] / 255, vertex.colour[2] / 255,
						vertex.colour[3] / 255);
					localNormals.push(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
				}

				worldPositions = deriveWorldPositions(localPositions, modelTransform);
				worldNormals = deriveWorldNormals(localNormals, modelTransform);
			}
			else
			{
				// The server omitted the vertexes because it was told this chunk's current vertex version is
				// already cached here, so reuse the cached local data. World-space geometry only needs
				// re-deriving if this chunk's model transform actually changed or it moved to a different
				// transform slot; otherwise the previous load's world-space arrays are still correct.
				localPositions = cached.localPositions;
				colors = cached.colors;
				localNormals = cached.localNormals;

				if(transformChanged[chunk.modelTransformIndex] || cached.modelTransformIndex !== chunk.modelTransformIndex)
				{
					worldPositions = deriveWorldPositions(localPositions, modelTransform);
					worldNormals = deriveWorldNormals(localNormals, modelTransform);
				}
				else
				{
					worldPositions = cached.worldPositions;
					worldNormals = cached.worldNormals;
				}
			}

			newChunkCache[cacheKey] = {

				vertexVersion: chunk.vertexVersion,
				modelTransformIndex: chunk.modelTransformIndex,
				localPositions: localPositions,
				colors: colors,
				localNormals: localNormals,
				worldPositions: worldPositions,
				worldNormals: worldNormals
			};

			// Only the always-on geometry contributes to picking and to the initial camera fit; hover-only
			// chunks must not steal picks or enlarge the framing.
			if(opaque)
			{
				for(var p = 0; p < worldPositions.length; p += 3)
				{
					pickPositions.push(worldPositions[p], worldPositions[p + 1], worldPositions[p + 2]);

					// Every third vertex completes another triangle belonging to this chunk.
					if((p / 3) % 3 === 2) pickTriangleChunkIds.push(chunk.id);

					minX = Math.min(minX, worldPositions[p]); maxX = Math.max(maxX, worldPositions[p]);
					minY = Math.min(minY, worldPositions[p + 1]); maxY = Math.max(maxY, worldPositions[p + 1]);
					minZ = Math.min(minZ, worldPositions[p + 2]); maxZ = Math.max(maxZ, worldPositions[p + 2]);

					if(chunkFocus)
					{
						hasFocus = true;

						focusMinX = Math.min(focusMinX, worldPositions[p]); focusMaxX = Math.max(focusMaxX, worldPositions[p]);
						focusMinY = Math.min(focusMinY, worldPositions[p + 1]); focusMaxY = Math.max(focusMaxY, worldPositions[p + 1]);
						focusMinZ = Math.min(focusMinZ, worldPositions[p + 2]); focusMaxZ = Math.max(focusMaxZ, worldPositions[p + 2]);
					}
				}
			}

			sceneChunks.push({ id: chunk.id, nodeId: chunk.nodeId, opaque: opaque, agent: agentChunk,
				positions: worldPositions, colors: colors, normals: worldNormals });
		}

		chunkCache = newChunkCache;
		lastModelTransforms = data.modelTransforms.map(function(t) { return t.transform.slice(); });

		lastPositions = pickPositions;
		lastTriangleChunkIds = pickTriangleChunkIds;
		lastChunkPokeable = chunkPokeable;

		var texts = data.texts || [];
		var newSceneTextElements = {};

		sceneTexts = [];

		for(var s = 0; s < texts.length; s++)
		{
			var text = texts[s];
			var textTransform = data.modelTransforms[text.modelTransformIndex].transform;

			// Reused across loads where the node is still there, so only what changed is touched. Filled in
			// with textContent alone: the text is whatever a script said, and must never be taken as markup.
			var el = sceneTextElements[text.nodeId];

			if(!el)
			{
				el = document.createElement('div');
				el.className = 'sceneText';
				sceneTextLayer.appendChild(el);
			}

			if(el.textContent !== text.text) el.textContent = text.text;

			el.style.color = cssColour(text.colour);

			applyTextBackground(el, text.background, text.height);

			newSceneTextElements[text.nodeId] = el;

			var worldPos = transformPoint(textTransform, text.position[0], text.position[1], text.position[2]);

			// The transform's scale along local up, so that text is scaled by a transform the same way the
			// geometry beside it is.
			var up = transformVec4(textTransform, 0, 1, 0, 0);

			var sceneText = {
				nodeId: text.nodeId,
				worldPos: [worldPos[0], worldPos[1], worldPos[2]],
				worldHeight: text.height * Math.sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]),
				el: el,
				shown: false,
				depth: 0,
				marker: false
			};

			buildMarker(sceneText, text.text, text.height, text.colour, text.background);

			// A text is framed by its marker, which has the text block's extent where its anchor alone has none.
			// The marker turns to face the camera, so whichever way it is turned it stays within its half
			// diagonal of the anchor.
			var markerReach = Math.sqrt(sceneText.markerHalfWidth * sceneText.markerHalfWidth +
				sceneText.markerHalfHeight * sceneText.markerHalfHeight);

			minX = Math.min(minX, worldPos[0] - markerReach); maxX = Math.max(maxX, worldPos[0] + markerReach);
			minY = Math.min(minY, worldPos[1] - markerReach); maxY = Math.max(maxY, worldPos[1] + markerReach);
			minZ = Math.min(minZ, worldPos[2] - markerReach); maxZ = Math.max(maxZ, worldPos[2] + markerReach);

			sceneTexts.push(sceneText);
		}

		for(var oldNodeId in sceneTextElements)
		{
			if(!newSceneTextElements[oldNodeId]) sceneTextLayer.removeChild(sceneTextElements[oldNodeId]);
		}

		sceneTextElements = newSceneTextElements;

		rebuildGeometry();

		status.textContent = vertexCount + ' vertexes across ' + data.chunks.length +
			' chunk(s). Drag to orbit, scroll to zoom, right-drag to pan, right-click a chunk to centre.' +
			(sceneTexts.length > 0 ? ' Right-click text to read it.' : '') +
			(structureLink ? ' Ctrl-click a chunk to find its node in the structure.' : '');

		if((vertexCount > 0 || sceneTexts.length > 0) && !cameraFitted)
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

				// Choose the distance so the focus node's bounding-sphere diameter spans focusViewportFraction of
				// the viewport height. At distance d the frustum half-height is d*tan(fovy/2); the sphere's
				// radius projects to that half-height when focusRadius = fraction * d*tan(fovy/2), so solve for d.
				var focusFraction = (typeof data.focusViewportFraction === 'number' && data.focusViewportFraction > 0) ?
					data.focusViewportFraction : 0.5;

				camera.distance = focusRadius / (focusFraction * Math.tan(CAMERA_FOVY / 2));
			}
			else
			{
				camera.centre = [(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2];

				camera.distance = radius * 2.5;
			}

			cameraFitted = true;
		}
	}

	// Concatenates the currently visible chunks into the GPU buffers: every opaque (ALWAYS) chunk first, then
	// each translucent chunk that something currently reveals - an AGENT chunk whose node the server has
	// marked as being worked on, or any other chunk whose owning node is hovered. opaqueVertexCount records
	// the boundary so draw() can render the two as separate passes. Called on load and whenever the hover
	// state changes.
	function rebuildGeometry()
	{
		var positions = [];
		var colors = [];
		var normals = [];

		for(var c = 0; c < sceneChunks.length; c++)
		{
			var chunk = sceneChunks[c];

			if(!chunk.opaque) continue;

			positions.push.apply(positions, chunk.positions);
			colors.push.apply(colors, chunk.colors);
			normals.push.apply(normals, chunk.normals);
		}

		opaqueVertexCount = positions.length / 3;

		for(var t = 0; t < sceneChunks.length; t++)
		{
			var transChunk = sceneChunks[t];

			if(transChunk.opaque) continue;

			// AGENT geometry is revealed by its node's server side flag rather than by the pointer, so it is
			// deliberately not shown on hover the way the remaining visibilities are.
			if(transChunk.agent ? !agentVisibleNodes[transChunk.nodeId] : transChunk.nodeId !== hoveredNodeId) continue;

			positions.push.apply(positions, transChunk.positions);
			colors.push.apply(colors, transChunk.colors);
			normals.push.apply(normals, transChunk.normals);
		}

		vertexCount = positions.length / 3;

		gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.STATIC_DRAW);

		scheduleDraw();
	}

	// True while a requestAnimationFrame(draw) call is outstanding, so scheduleDraw() can be called freely
	// from every input handler and state change without ever stacking up more than one pending frame.
	var rafPending = false;

	// Requests a single redraw. draw() only reschedules itself while an animation (the recentre tween) is
	// still in flight, so the render loop otherwise goes idle between input events instead of spinning at
	// the display refresh rate while the scene is unchanged.
	function scheduleDraw()
	{
		if(rafPending) return;

		rafPending = true;
		requestAnimationFrame(draw);
	}

	function draw()
	{
		rafPending = false;

		if(centreAnimTo !== null || distanceAnimTo !== null)
		{
			var animT = Math.min(1, (performance.now() - centreAnimStartTime) / CENTRE_ANIM_MS);

			// Smoothstep: eases in and out instead of the tween starting/stopping abruptly.
			var eased = animT * animT * (3 - 2 * animT);

			if(centreAnimTo !== null)
			{
				camera.centre = [
					centreAnimFrom[0] + (centreAnimTo[0] - centreAnimFrom[0]) * eased,
					centreAnimFrom[1] + (centreAnimTo[1] - centreAnimFrom[1]) * eased,
					centreAnimFrom[2] + (centreAnimTo[2] - centreAnimFrom[2]) * eased
				];

				if(animT >= 1) centreAnimTo = null;
			}

			if(distanceAnimTo !== null)
			{
				// Interpolated by ratio rather than difference, so a zoom across several orders of magnitude
				// moves at an even pace instead of rushing through the far end of it.
				camera.distance = distanceAnimFrom * Math.pow(distanceAnimTo / distanceAnimFrom, eased);

				if(animT >= 1) distanceAnimTo = null;
			}
		}

		gl.clearColor(0.125, 0.125, 0.125, 1);
		gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
		gl.enable(gl.DEPTH_TEST);

		// A scene can hold nothing but text, which still needs a camera to be placed by.
		if(vertexCount > 0 || sceneTexts.length > 0)
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

			// The camera sits camera.distance back along its own +z from the centre it orbits, i.e. the
			// inverse of the view matrix built above.
			if(uCameraPos)
			{
				var cameraBack = transformNormal(invViewRot, 0, 0, 1);

				gl.uniform3f(uCameraPos,
					camera.centre[0] + cameraBack[0] * camera.distance,
					camera.centre[1] + cameraBack[1] * camera.distance,
					camera.centre[2] + cameraBack[2] * camera.distance);
			}

			updateSceneTexts(viewProj,
				transformNormal(invViewRot, 1, 0, 0),
				transformNormal(invViewRot, 0, 1, 0),
				transformNormal(invViewRot, 0, 0, 1));

			// Opaque pass: the ALWAYS-visible geometry, written to the depth buffer as usual.
			gl.disable(gl.BLEND);
			gl.depthMask(true);

			if(opaqueVertexCount > 0)
			{
				bindAttributes(positionBuffer, colorBuffer, normalBuffer);
				gl.drawArrays(gl.TRIANGLES, 0, opaqueVertexCount);
			}

			// Text markers are part of the opaque scene, so geometry in front of one hides it.
			if(markerVertexCount > 0)
			{
				bindAttributes(markerPositionBuffer, markerColorBuffer, markerNormalBuffer);
				gl.drawArrays(gl.TRIANGLES, 0, markerVertexCount);
			}

			// Translucent pass: any hover-revealed geometry (e.g. a halo), alpha-blended over the opaque scene.
			// Depth writes are disabled so overlapping translucent triangles don't cull one another, while the
			// depth test stays on so opaque geometry in front still occludes it.
			if(vertexCount > opaqueVertexCount)
			{
				bindAttributes(positionBuffer, colorBuffer, normalBuffer);

				gl.enable(gl.BLEND);
				gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
				gl.depthMask(false);

				gl.drawArrays(gl.TRIANGLES, opaqueVertexCount, vertexCount - opaqueVertexCount);

				gl.depthMask(true);
				gl.disable(gl.BLEND);
			}
		}

		// Keep rendering every frame while the recentre tween is in flight; otherwise leave the loop idle
		// until the next scheduleDraw() call from an input handler or state change.
		if(centreAnimTo !== null || distanceAnimTo !== null) scheduleDraw();
	}

	var dataUrl = window.location.pathname.replace(/\/+$/, '') + '/data';
	var viewers = document.getElementById('viewers');

	// Names this page load to the server, which counts the pages polling it as the people currently viewing
	// the scene. Made up here rather than handed out by the server, so that loading the page costs no extra
	// round trip; a collision only undercounts by one.
	var viewerId = (window.crypto && window.crypto.randomUUID) ? window.crypto.randomUUID() :
		Math.random().toString(36).slice(2) + Date.now().toString(36);

	var revisionUrl = window.location.pathname.replace(/\/+$/, '') + '/revision?viewerId=' +
		encodeURIComponent(viewerId);
	var lastRevision = null;
	var pollTimer = null;

	// Without this a closed page would go on being counted until the server expires it a few polls later.
	window.addEventListener('pagehide', function()
	{
		if(navigator.sendBeacon) navigator.sendBeacon(revisionUrl + '&leave=1');
	});

	function showViewerCount(viewerCount)
	{
		if(typeof viewerCount !== 'number') return;

		viewers.textContent = viewerCount + (viewerCount === 1 ? ' viewer' : ' viewers');
	}

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
		viewers.textContent = '';
	}

	function poll()
	{
		fetch(revisionUrl).then(function(res) { return res.json(); }).then(function(revisionData)
		{
			// Ahead of the unchanged revision check, as who is viewing changes independently of the scene.
			showViewerCount(revisionData.viewerCount);

			if(revisionData.revision === lastRevision) return null;

			console.log('Scene revision changed: ' + lastRevision + ' -> ' + revisionData.revision);

			lastRevision = revisionData.revision;

			// Tells the server which chunks are already cached here, and at which vertex version, so it can
			// omit vertexes for any chunk that hasn't changed instead of resending the whole scene.
			var knownChunks = Object.keys(chunkCache).map(function(key)
			{
				var entry = chunkCache[key];
				var separatorIndex = key.indexOf(':');

				return {

					nodeId: parseInt(key.slice(0, separatorIndex), 10),
					chunkId: parseInt(key.slice(separatorIndex + 1), 10),
					vertexVersion: entry.vertexVersion
				};
			});

			return fetch(dataUrl, {

				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ chunks: knownChunks })

			}).then(function(res) { return res.json(); }).then(function(data)
			{
				loadScene(data);
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
<script>
%CHAT_SCRIPT%
</script>
</body>
</html>
)HTMLPAGE";
