#include "webglPageTemplate.hpp"

// The WebGL viewer page. It fetches this map's data endpoint and renders the returned chunks as triangles,
// applying each chunk's model transform on the client before upload so that only a single camera uniform is
// needed to draw the whole scene. It polls the map's revision endpoint to detect a changed or replaced surface,
// only re-fetching scene data when the revision actually changes. The data fetch itself is incremental: the
// viewer tells the server which chunks it already holds vertex data for (by id and vertex version), the server
// omits vertexes for any chunk that hasn't changed, and the viewer reuses its cached per-chunk geometry -
// re-deriving world-space positions/normals only for chunks whose model transform actually changed - instead of
// reprocessing the whole scene on every poll.
const char* const webglPageTemplate = R"HTMLPAGE(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>%TITLE%</title>
<style>
	html, body { margin: 0; padding: 0; overflow: hidden; background: #202020; }
	canvas { display: block; width: 100vw; height: 100vh; }
	#status { position: absolute; top: 8px; left: 8px; color: #ddd; font-family: sans-serif; font-size: 13px; }

	/* -- Chat window -- */

	#chatToggle { position: absolute; right: 12px; bottom: 12px; padding: 6px 12px; color: #ddd;
		background: #303030; border: 1px solid #4a4a4a; border-radius: 4px; font-family: sans-serif;
		font-size: 13px; cursor: pointer; }
	#chatToggle:hover { background: #3a3a3a; }
	#chatWindow { position: absolute; display: flex; flex-direction: column; width: 340px; height: 430px;
		background: #282828; border: 1px solid #4a4a4a; border-radius: 5px; box-shadow: 0 6px 18px rgba(0, 0, 0, 0.5);
		color: #ddd; font-family: sans-serif; font-size: 13px; overflow: hidden; }
	#chatWindow[hidden] { display: none; }
	#chatHeader { display: flex; align-items: center; padding: 6px 8px; background: #333333; cursor: move;
		border-bottom: 1px solid #4a4a4a; user-select: none; -webkit-user-select: none; }
	#chatHeader span { flex: 1; }
	#chatBar { display: flex; padding: 6px 8px; border-bottom: 1px solid #383838; }
	#chatBar > * { margin-right: 4px; }
	#chatBar > *:last-child { margin-right: 0; }
	#chatContextSelect { flex: 1; min-width: 0; padding: 2px; color: #ddd; background: #303030;
		border: 1px solid #4a4a4a; border-radius: 3px; font-family: inherit; font-size: 12px; }
	#chatLog { flex: 1; padding: 8px; overflow-y: auto; }
	#chatInputRow { display: flex; padding: 8px; border-top: 1px solid #383838; }
	#chatPromptInput { flex: 1; min-width: 0; margin-right: 4px; padding: 4px; color: #ddd; background: #303030;
		border: 1px solid #4a4a4a; border-radius: 3px; font-family: inherit; font-size: 12px; resize: none; }
	#chatWindow button { padding: 3px 8px; color: #ddd; background: #383838; border: 1px solid #4a4a4a;
		border-radius: 3px; font-family: inherit; font-size: 12px; cursor: pointer; }
	#chatWindow button:hover:enabled { background: #444444; }
	#chatWindow button:disabled { color: #777; cursor: default; }
	.chatEntry { margin-bottom: 8px; line-height: 1.35; white-space: pre-wrap; overflow-wrap: break-word; }
	.chatEntry .chatWho { display: block; margin-bottom: 2px; color: #888; font-size: 11px; }
	.chatPrompt .chatBody { color: #cfe3ff; }
	.chatError .chatBody { color: #ff9a8a; }
	.chatNote { color: #888; font-style: italic; }
</style>
</head>
<body>
<div id="status">Loading...</div>
<canvas id="glCanvas"></canvas>
<button id="chatToggle">Chat</button>
<div id="chatWindow" hidden>
	<div id="chatHeader"><span>Chat</span><button id="chatClose" title="Close">X</button></div>
	<div id="chatBar">
		<select id="chatContextSelect" title="Conversation"></select>
		<button id="chatNew" title="Start a new conversation">New</button>
		<button id="chatRemove" title="Discard this conversation">Discard</button>
	</div>
	<div id="chatLog"></div>
	<div id="chatInputRow">
		<textarea id="chatPromptInput" rows="2" placeholder="Ask about this hive..."></textarea>
		<button id="chatSend">Send</button>
	</div>
</div>
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
	// Only ALWAYS-visible chunks go in here, so hover-revealed geometry never intercepts a pick.
	var lastPositions = [];
	var lastTriangleChunkIds = [];
	var lastChunkPokeable = {};
	var lastViewProj = null;

	// The most recently loaded scene, split per chunk with world-space geometry precomputed, so that only the
	// currently visible chunks need be concatenated and uploaded whenever the hover state changes. Each entry is
	// { id, nodeId, opaque, positions, colors, normals }; opaque means VertexVisibility.ALWAYS.
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

	// Pokes the surface backing this map to say the chunk with the given id was interacted with. A chunk id is
	// only unique within its owning node, so the owning node id is sent alongside it to identify the chunk.
	// type is omitted for a click (HIT, the server's default) or passed as 'hoverEnter'/'hoverLeave' when the
	// pointer moves onto or off of a pokeable chunk.
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

		var picked = pickChunkAt(e.clientX, e.clientY);

		if(picked !== null && lastChunkPokeable[picked.chunkId])
		{
			// Start from wherever the centre currently is, which is the tween's midpoint if a previous
			// recentre is still in flight, so a fast double pick doesn't stutter back to the old target.
			centreAnimFrom = camera.centre.slice();
			centreAnimTo = picked.point;
			centreAnimStartTime = performance.now();

			scheduleDraw();
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

			sceneChunks.push({ id: chunk.id, nodeId: chunk.nodeId, opaque: opaque,
				positions: worldPositions, colors: colors, normals: worldNormals });
		}

		chunkCache = newChunkCache;
		lastModelTransforms = data.modelTransforms.map(function(t) { return t.transform.slice(); });

		lastPositions = pickPositions;
		lastTriangleChunkIds = pickTriangleChunkIds;
		lastChunkPokeable = chunkPokeable;

		rebuildGeometry();

		status.textContent = vertexCount + ' vertexes across ' + data.chunks.length + ' chunk(s). Drag to orbit, scroll to zoom, right-drag to pan, right-click a chunk to centre.';

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
	// any translucent chunk whose owning node is currently hovered. opaqueVertexCount records the boundary so
	// draw() can render the two as separate passes. Called on load and whenever the hover state changes.
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

			if(transChunk.opaque || transChunk.nodeId !== hoveredNodeId) continue;

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

			// Opaque pass: the ALWAYS-visible geometry, written to the depth buffer as usual.
			gl.disable(gl.BLEND);
			gl.depthMask(true);
			gl.drawArrays(gl.TRIANGLES, 0, opaqueVertexCount);

			// Translucent pass: any hover-revealed geometry (e.g. a halo), alpha-blended over the opaque scene.
			// Depth writes are disabled so overlapping translucent triangles don't cull one another, while the
			// depth test stays on so opaque geometry in front still occludes it.
			if(vertexCount > opaqueVertexCount)
			{
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
		if(centreAnimTo !== null) scheduleDraw();
	}

	var dataUrl = window.location.pathname.replace(/\/+$/, '') + '/data';
	var revisionUrl = window.location.pathname.replace(/\/+$/, '') + '/revision';
	var lastRevision = null;
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

	// -- Chat window --
	//
	// A floating panel over the scene, dragged around by its header, onto the chat interface of the surface this
	// map is bound to. A prompt takes as long as the model takes, which is far too long to hold an HTTP request
	// open for, so the server accepts a prompt and answers with an id to poll for the reply. That leaves the
	// scene's own polling above free to carry on rendering while a chat is being answered.

	var chatBase = window.location.pathname.replace(/\/+$/, '') + '/chat';
	var chatMessageUrl = chatBase + '/message';
	var chatContextsUrl = chatBase + '/contexts';
	var chatRemoveContextUrl = chatBase + '/removeContext';

	var CHAT_POLL_INTERVAL_MS = 500;

	var chatToggle = document.getElementById('chatToggle');
	var chatWindow = document.getElementById('chatWindow');
	var chatHeader = document.getElementById('chatHeader');
	var chatClose = document.getElementById('chatClose');
	var chatContextSelect = document.getElementById('chatContextSelect');
	var chatNew = document.getElementById('chatNew');
	var chatRemove = document.getElementById('chatRemove');
	var chatLog = document.getElementById('chatLog');
	var chatPromptInput = document.getElementById('chatPromptInput');
	var chatSend = document.getElementById('chatSend');

	// Id of the conversation on show, or null when the next prompt is to start a fresh one.
	var chatContextId = null;

	// What was said in each conversation this page has taken part in, keyed by conversation id, plus the one for
	// a conversation not yet started (which has no id to key it by until its first prompt is answered). The
	// server holds the conversations themselves and this is only what to show of them, so one started before this
	// page was loaded shows as empty until something further is said in it.
	var chatTranscripts = {};
	var chatNewTranscript = [];

	// Id of the prompt currently with the server, or null. Only ever one at a time, since a conversation refuses
	// a prompt made while it is still answering an earlier one.
	var chatPendingMessageId = null;

	function chatTranscript()
	{
		if(chatContextId === null) return chatNewTranscript;

		if(!chatTranscripts[chatContextId]) chatTranscripts[chatContextId] = [];

		return chatTranscripts[chatContextId];
	}

	function renderChatLog()
	{
		var transcript = chatTranscript();

		chatLog.textContent = '';

		if(transcript.length === 0)
		{
			var note = document.createElement('div');

			note.className = 'chatEntry chatNote';
			note.textContent = (chatContextId === null) ?
				'Nothing said yet. Ask something to start a conversation.' :
				'This conversation was not started on this page, so what was already said in it is not shown here.';

			chatLog.appendChild(note);
		}

		for(var i = 0; i < transcript.length; i++)
		{
			var entry = transcript[i];

			var element = document.createElement('div');

			element.className = 'chatEntry ' + entry.kind;

			var who = document.createElement('span');

			who.className = 'chatWho';
			who.textContent = entry.who;

			var body = document.createElement('span');

			body.className = 'chatBody';

			// textContent rather than innerHTML throughout: a reply is whatever the model chose to say, and must
			// never be treated as markup by this page.
			body.textContent = entry.text;

			element.appendChild(who);
			element.appendChild(body);

			chatLog.appendChild(element);
		}

		if(chatPendingMessageId !== null)
		{
			var pending = document.createElement('div');

			pending.className = 'chatEntry chatNote';
			pending.textContent = 'Thinking...';

			chatLog.appendChild(pending);
		}

		chatLog.scrollTop = chatLog.scrollHeight;
	}

	function addChatEntry(kind, who, text)
	{
		chatTranscript().push({ kind: kind, who: who, text: text });

		renderChatLog();
	}

	// Everything that would change which conversation a reply belongs to is shut off while a prompt is with the
	// server, so the reply can only ever land in the transcript the prompt was made from.
	function setChatBusy(busy)
	{
		chatSend.disabled = busy;
		chatNew.disabled = busy;
		chatContextSelect.disabled = busy;
		chatRemove.disabled = busy || chatContextId === null;

		renderChatLog();
	}

	function startNewChatConversation()
	{
		chatContextId = null;
		chatNewTranscript = [];
		chatContextSelect.value = '';

		setChatBusy(false);
	}

	function refreshChatContexts()
	{
		fetch(chatContextsUrl).then(function(res) { return res.json(); }).then(function(data)
		{
			var contexts = data.contexts || [];

			chatContextSelect.textContent = '';

			var newOption = document.createElement('option');

			newOption.value = '';
			newOption.textContent = 'New conversation';

			chatContextSelect.appendChild(newOption);

			var onShowStillHeld = false;

			for(var i = 0; i < contexts.length; i++)
			{
				var option = document.createElement('option');

				option.value = String(contexts[i].id);

				// A conversation describes itself by what was first asked of it, which is empty only for one
				// whose first prompt hasn't been answered yet.
				option.textContent = contexts[i].description || ('Conversation ' + contexts[i].id);

				chatContextSelect.appendChild(option);

				if(contexts[i].id === chatContextId) onShowStillHeld = true;
			}

			// A conversation the surface no longer holds (discarded from another page, or from a surface that has
			// since been replaced) can't be carried on, so fall back to starting a fresh one rather than leaving a
			// selection that would be refused.
			if(chatContextId !== null && !onShowStillHeld) startNewChatConversation();

			chatContextSelect.value = (chatContextId === null) ? '' : String(chatContextId);

		}).catch(function(err)
		{
			// Nothing to do but leave the list as it was; the next refresh will pick it up.
		});
	}

	// Reads a JSON response along with whether it was an error, so a refusal can be reported by what the server
	// said rather than by its status code alone.
	function chatJson(res)
	{
		return res.json().then(function(data) { return { ok: res.ok, data: data }; });
	}

	function chatError(result, fallback)
	{
		return new Error((result.data && result.data.error) ? result.data.error : fallback);
	}

	function pollChatMessage()
	{
		if(chatPendingMessageId === null) return;

		fetch(chatMessageUrl + '?messageId=' + chatPendingMessageId).then(chatJson).then(function(result)
		{
			if(!result.ok) throw chatError(result, 'The server no longer knows about that prompt.');

			if(result.data.state === 'pending')
			{
				setTimeout(pollChatMessage, CHAT_POLL_INTERVAL_MS);
				return;
			}

			chatPendingMessageId = null;

			if(result.data.state === 'answered')
			{
				// A prompt that started a conversation is answered with the id it was given, which is what every
				// prompt after it has to be sent with to stay in the same conversation. Adopted before the reply
				// is added so that the reply lands in that conversation's transcript.
				if(chatContextId === null && typeof result.data.contextId === 'number')
				{
					chatContextId = result.data.contextId;
					chatTranscripts[chatContextId] = chatNewTranscript;
					chatNewTranscript = [];
				}

				addChatEntry('chatReply', 'Model', result.data.reply);

				refreshChatContexts();
			}
			else
			{
				addChatEntry('chatError', 'Error', result.data.error || 'The chat could not be serviced.');
			}

			setChatBusy(false);

		}).catch(function(err)
		{
			chatPendingMessageId = null;

			addChatEntry('chatError', 'Error', String(err.message || err));

			setChatBusy(false);
		});
	}

	function sendChatPrompt()
	{
		var prompt = chatPromptInput.value.trim();

		if(prompt === '' || chatPendingMessageId !== null) return;

		var body = { prompt: prompt };

		// Leaving the conversation id out of the body is what asks for a fresh conversation to be started.
		if(chatContextId !== null) body.contextId = chatContextId;

		chatPromptInput.value = '';

		addChatEntry('chatPrompt', 'You', prompt);

		setChatBusy(true);

		fetch(chatBase, {

			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(body)

		}).then(chatJson).then(function(result)
		{
			if(!result.ok || typeof result.data.messageId !== 'number')
			{
				throw chatError(result, 'The prompt was refused.');
			}

			chatPendingMessageId = result.data.messageId;

			renderChatLog();

			setTimeout(pollChatMessage, CHAT_POLL_INTERVAL_MS);

		}).catch(function(err)
		{
			addChatEntry('chatError', 'Error', String(err.message || err));

			setChatBusy(false);
		});
	}

	chatSend.addEventListener('click', sendChatPrompt);

	chatPromptInput.addEventListener('keydown', function(e)
	{
		// Enter sends, shift-enter is a line break, as everywhere else that has a prompt box.
		if(e.key !== 'Enter' || e.shiftKey) return;

		e.preventDefault();

		sendChatPrompt();
	});

	chatNew.addEventListener('click', startNewChatConversation);

	chatContextSelect.addEventListener('change', function()
	{
		chatContextId = (chatContextSelect.value === '') ? null : parseInt(chatContextSelect.value, 10);

		if(chatContextId === null) chatNewTranscript = [];

		setChatBusy(false);
	});

	chatRemove.addEventListener('click', function()
	{
		if(chatContextId === null) return;

		var removedContextId = chatContextId;

		fetch(chatRemoveContextUrl + '?contextId=' + removedContextId, { method: 'POST' }).then(chatJson)
			.then(function(result)
		{
			if(!result.ok) throw chatError(result, 'The conversation could not be discarded.');

			delete chatTranscripts[removedContextId];

			startNewChatConversation();
			refreshChatContexts();

		}).catch(function(err)
		{
			addChatEntry('chatError', 'Error', String(err.message || err));
		});
	});

	// -- Chat window placement --
	//
	// Dragged around by its header. The canvas' own orbit/pan handlers are bound to the canvas itself, so they
	// never see a drag that started on this window, and the window's position is clamped to the viewport so it
	// can't be dropped somewhere it could no longer be grabbed from.

	var chatDragging = false;
	var chatDragOffsetX = 0, chatDragOffsetY = 0;

	function placeChatWindow(left, top)
	{
		var maxLeft = Math.max(0, window.innerWidth - chatWindow.offsetWidth);
		var maxTop = Math.max(0, window.innerHeight - chatWindow.offsetHeight);

		chatWindow.style.left = Math.max(0, Math.min(maxLeft, left)) + 'px';
		chatWindow.style.top = Math.max(0, Math.min(maxTop, top)) + 'px';
	}

	chatHeader.addEventListener('mousedown', function(e)
	{
		if(e.button !== 0) return;

		var rect = chatWindow.getBoundingClientRect();

		chatDragging = true;
		chatDragOffsetX = e.clientX - rect.left;
		chatDragOffsetY = e.clientY - rect.top;

		// Stops the drag selecting the header's text instead of moving the window.
		e.preventDefault();
	});

	window.addEventListener('mousemove', function(e)
	{
		if(!chatDragging) return;

		placeChatWindow(e.clientX - chatDragOffsetX, e.clientY - chatDragOffsetY);
	});

	window.addEventListener('mouseup', function() { chatDragging = false; });

	window.addEventListener('resize', function()
	{
		if(chatWindow.hidden) return;

		placeChatWindow(chatWindow.offsetLeft, chatWindow.offsetTop);
	});

	chatToggle.addEventListener('click', function()
	{
		if(!chatWindow.hidden)
		{
			// Only hidden, not reset: a prompt still with the server keeps being polled, so its reply is waiting
			// in the transcript when the window is opened again.
			chatWindow.hidden = true;
			return;
		}

		chatWindow.hidden = false;

		// Opens in the top right corner, clear of the status text in the opposite one. Only on the first open,
		// so it comes back wherever it was last dragged to after that.
		if(!chatWindow.style.left) placeChatWindow(window.innerWidth - chatWindow.offsetWidth - 12, 12);

		refreshChatContexts();
		renderChatLog();

		chatPromptInput.focus();
	});

	chatClose.addEventListener('click', function() { chatWindow.hidden = true; });

	setChatBusy(false);
})();
</script>
</body>
</html>
)HTMLPAGE";
