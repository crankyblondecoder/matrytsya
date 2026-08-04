#include "graphViewPageTemplate.hpp"

// The graph a hive reports carries no positions, only which nodes there are and which node each edge leads to,
// so this page has to decide where to put things itself. It does that with a force-directed layout: edges pull
// the nodes they join together, every node pushes every other apart, and the whole thing is drawn towards the
// origin so it cannot drift away. The layout is run hard for a moment when a graph arrives and then animated to
// a stop, after which the render loop goes idle until the pointer moves or the graph changes.
// Positions are kept against node ids and survive a reload, so a node that is still there stays where it was
// and only what actually changed moves. That matters because the point of the view is to recognise the shape of
// a hive, which it would not be if every poll reshuffled it.
// Nothing on this page is drawn as glyphs in the scene. What a node or an edge is gets told in an HTML panel
// positioned over the canvas, which is why the scene itself is only spheres and tubes.

const char* const graphViewPageTemplate = R"HTMLPAGE(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>%TITLE%</title>
<style>
	html, body { margin: 0; padding: 0; overflow: hidden; background: #202020; }
	canvas { display: block; width: 100vw; height: 100vh; }
	#status { position: absolute; top: 8px; left: 8px; color: #ddd; font-family: sans-serif; font-size: 13px; }
	#tooltip, #callout { position: absolute; top: 0; left: 0; max-width: 320px; padding: 6px 9px;
		background: #282828; border: 1px solid #4a4a4a; border-radius: 4px; color: #ddd;
		font-family: sans-serif; font-size: 12px; pointer-events: none; white-space: pre; }
	#callout { border-color: #cfe3ff; }
	#callout[hidden] { display: none; }
	#calloutTitle { color: #cfe3ff; font-weight: bold; }
	#calloutBody { color: #bbb; margin-top: 2px; }
	#legend { position: absolute; top: 34px; left: 8px; padding: 6px 9px; background: #282828;
		border: 1px solid #4a4a4a; border-radius: 4px; color: #ddd; font-family: sans-serif; font-size: 11px;
		pointer-events: none; }
	#legend[hidden] { display: none; }
	.legendRow { display: flex; align-items: center; margin-bottom: 3px; }
	.legendRow:last-child { margin-bottom: 0; }
	.legendSwatch { width: 11px; height: 11px; margin-right: 7px; flex: none; }
	.legendSphere { border-radius: 50%; }
	.legendCube { border-radius: 1px; }
	.legendTetrahedron { clip-path: polygon(50% 0%, 100% 100%, 0% 100%); }
	.legendDodecahedron { clip-path: polygon(50% 0%, 100% 38%, 82% 100%, 18% 100%, 0% 38%); }
	#tooltipTitle { color: #cfe3ff; font-weight: bold; }
	#tooltipBody { color: #bbb; margin-top: 2px; }
	#backLink { position: absolute; right: 12px; bottom: 12px; padding: 6px 12px; color: #ddd;
		background: #303030; border: 1px solid #4a4a4a; border-radius: 4px; font-family: sans-serif;
		font-size: 13px; text-decoration: none; cursor: pointer; }
	#backLink:hover { background: #3a3a3a; }
	#backLink[hidden] { display: none; }
%CHAT_STYLE%
</style>
</head>
<body>
<div id="status">Loading...</div>
<div id="legend" hidden></div>
<canvas id="glCanvas"></canvas>
<div id="tooltip" hidden><div id="tooltipTitle"></div><div id="tooltipBody"></div></div>
<div id="callout" hidden><div id="calloutTitle"></div><div id="calloutBody"></div></div>
<a id="backLink" hidden></a>
%CHAT_MARKUP%
<script>
(function() {
	'use strict';

	var canvas = document.getElementById('glCanvas');
	var status = document.getElementById('status');
	var legend = document.getElementById('legend');
	var tooltip = document.getElementById('tooltip');
	var tooltipTitle = document.getElementById('tooltipTitle');
	var tooltipBody = document.getElementById('tooltipBody');
	var callout = document.getElementById('callout');
	var calloutTitle = document.getElementById('calloutTitle');
	var calloutBody = document.getElementById('calloutBody');

	// A node this page was sent here to point out, named in the address by whatever sent it. Held until the
	// pointer is brought back onto that node, at which point it has been found and saying so again would only
	// be in the way of what the pointer is now doing.
	var calloutNodeId = (function()
	{
		var match = /[?&]node=(\d+)/.exec(window.location.search);

		return match ? Number(match[1]) : null;
	})();

	// The way back is only offered when this page was jumped to from a surface that said which one it was.
	// Opened on its own, from the index or from a bookmark, there is no particular surface to go back to and
	// no button appears. Done before the WebGL check below, so a browser that cannot draw the graph can still
	// be used to get back to where it came from.
	(function()
	{
		var backLink = document.getElementById('backLink');
		var fromMatch = /[?&]from=([^&]*)/.exec(window.location.search);

		if(!backLink || !fromMatch) return;

		var fromPath = decodeURIComponent(fromMatch[1]);

		// Only ever a path on this server. This arrived in the address bar, so it does not get to say where
		// pressing the button sends someone: anything that is not a plain rooted path, or that looks like the
		// start of another host, is ignored rather than followed.
		if(!/^\/[A-Za-z0-9._~!$&'()*+,;=:@%/-]*$/.test(fromPath) || fromPath.indexOf('//') === 0) return;

		// Named by the last part of its path, which is the surface's own name, so the button says where it
		// goes rather than just that it goes back.
		var segments = fromPath.replace(/\/+$/, '').split('/');
		var label = segments[segments.length - 1];

		backLink.href = fromPath;
		backLink.textContent = '← ' + (label || 'Back');
		backLink.title = 'Back to ' + fromPath;
		backLink.hidden = false;

		var chatToggle = document.getElementById('chatToggle');

		// Moved clear of the chat toggle, which shares this corner. Measured rather than assumed, so the two
		// stay side by side however wide the chat toggle's own label makes it.
		if(chatToggle) backLink.style.right = (chatToggle.offsetWidth + 20) + 'px';
	})();

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
	// to unproject a hovered screen point back into a world space picking ray.
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

	// -- What a node of each kind looks like --

	var NODE_RADIUS = 0.42;
	var EDGE_RADIUS = 0.055;
	var ARROW_RADIUS = 0.16;
	var ARROW_LENGTH = 0.34;

	// A colour per node type, so what a node is can be told apart before it is hovered over. Anything not
	// named here falls back to the plain node colour, which is what a type this page has not been taught about
	// gets rather than being left invisible.
	var NODE_COLOURS = {

		GRAPH_NODE: [0.62, 0.63, 0.68],
		PING_NODE: [0.40, 0.72, 0.95],
		SCENE_GEOMETRY_NODE: [0.42, 0.82, 0.50],
		SCENE_TRANSFORM_NODE: [0.95, 0.74, 0.34],
		SCRIPT_NODE: [0.76, 0.54, 0.95],
		SCENE_GEOMETRY_SCRIPT_NODE: [0.32, 0.66, 0.42],
		SCENE_TRANSFORM_SCRIPT_NODE: [0.85, 0.57, 0.24],
		SCENE_ROOT_NODE: [0.94, 0.94, 0.94],
		TELEPORT_NODE: [0.94, 0.44, 0.76],
		AGENT_NODE: [0.94, 0.42, 0.38],
		TRIGGER_NODE: [0.32, 0.84, 0.84]
	};

	/// What an edge that restricts nothing is drawn in. It lets every kind of action through rather than any
	/// one of them, so it is left plain rather than being given a colour of its own.
	// Node types drawn as a cube rather than a sphere. A geometry node is what actually puts something into a
	// scene, so it is given a shape of its own to pick it out from the nodes that only arrange, drive or
	// watch them. Built to the same radius as a sphere, so an edge still meets a face of it squarely.
	var CUBE_NODE_TYPES = {

		SCENE_GEOMETRY_NODE: true,
		SCENE_GEOMETRY_SCRIPT_NODE: true
	};

	// Node types drawn as a tetrahedron. A transform node places what hangs below it in a scene rather than
	// putting anything there itself, so it gets a shape of its own as well.
	var TETRAHEDRON_NODE_TYPES = {

		SCENE_TRANSFORM_NODE: true,
		SCENE_TRANSFORM_SCRIPT_NODE: true
	};

	/// How far a tetrahedron's points reach from its centre. Further out than a sphere's surface, because a
	/// tetrahedron only fills a fifth of the ball it fits inside and one built to the same reach would look
	/// far smaller than every other node rather than merely different.
	var TETRAHEDRON_RADIUS = NODE_RADIUS * 1.45;

	// Node types drawn as a dodecahedron. An agent node is where a hive stops following its own wiring and
	// asks a model what to do, which is worth being able to find at a glance.
	var DODECAHEDRON_NODE_TYPES = {

		AGENT_NODE: true
	};

	/// How far a dodecahedron's points reach from its centre. Barely more than a sphere's, since a
	/// dodecahedron very nearly fills the ball it fits inside.
	var DODECAHEDRON_RADIUS = NODE_RADIUS * 1.1;

	// How far it is from a node's centre to its surface, along the direction asked about. Only a sphere is
	// the same distance out whichever way it is asked: a cube reaches almost twice as far at a corner as at
	// a face, and a tetrahedron three times as far at a point. An edge has to be cut back to where the shape
	// it meets actually is along the line it arrives on, since cutting every edge to the nearest that
	// surface ever comes would leave the arrowhead buried inside anything but a sphere.
	// @param direction Unit vector from the node's centre towards where the surface is being asked about.
	function nodeReach(node, direction)
	{
		if(!node) return NODE_RADIUS;

		var scale = nodeScale(node);

		if(CUBE_NODE_TYPES[node.type])
		{
			return planeReach(CUBE_NORMALS, NODE_RADIUS * scale, direction);
		}

		if(TETRAHEDRON_NODE_TYPES[node.type])
		{
			return planeReach(TETRAHEDRON_NORMALS, TETRAHEDRON_RADIUS * TETRAHEDRON_INRADIUS * scale, direction);
		}

		if(DODECAHEDRON_NODE_TYPES[node.type])
		{
			return planeReach(DODECAHEDRON_NORMALS, DODECAHEDRON_RADIUS * DODECAHEDRON_INRADIUS * scale, direction);
		}

		// A sphere is the same distance out whichever way it is asked.
		return NODE_RADIUS * scale;
	}

	// How much bigger a node is drawn for the number of edges meeting it, counting those leading away from it
	// and those arriving at it alike. What is proportional to that number is how much shape there is, not how
	// wide it is, so the width goes as the cube root: a node with eight times the edges of another is drawn
	// twice as wide, and so has eight times the bulk. Taking the width itself as proportional would have a
	// well connected node swallow the graph around it rather than stand out in it.
	function nodeScale(node)
	{
		return Math.pow(Math.max(1, node.degree || 0), 1 / 3);
	}

	var EDGE_COLOUR = [0.44, 0.45, 0.50];

	// A colour per action flag, so that what an edge lets through can be told at a glance rather than only by
	// hovering it. These are the flag names a hive file uses, which is what the panel names them by too.
	var ACTION_COLOURS = {

		PING_GRAPH_ACTION: [0.35, 0.70, 1.00],
		SERIALISABLE_GRAPH_ACTION: [0.62, 0.56, 1.00],
		SCRIPT_GRAPH_ACTION: [0.82, 0.45, 1.00],
		SCENE_GRAPH_ACTION: [0.35, 0.90, 0.62],
		SCENE_STROBE_GRAPH_ACTION: [0.60, 0.95, 0.35],
		ANIMATE_GRAPH_ACTION: [1.00, 0.80, 0.30],
		VERSION_GRAPH_ACTION: [0.40, 0.88, 0.88],
		AGENT_GRAPH_ACTION: [1.00, 0.45, 0.40],
		TRIGGER_GRAPH_ACTION: [1.00, 0.62, 0.20],
		AGENT_AFFECT_GRAPH_ACTION: [1.00, 0.45, 0.78]
	};

	function nodeColour(type)
	{
		return NODE_COLOURS[type] || NODE_COLOURS.GRAPH_NODE;
	}

	// How a node's type is written wherever this page shows one. Every one of these names ends in _NODE,
	// which says nothing beyond what the thing being named already says. The names arrive whole, and are
	// still whole in what the server serves, so nothing but the reading of them is shortened here.
	function nodeTypeLabel(type)
	{
		return type.replace(/_NODE$/, '');
	}

	// The colour an edge is drawn in, from the action flags it carries. An edge carrying more than one is
	// drawn in the average of their colours, since it really does let all of them through; exactly which is
	// what the panel is for. An edge whose flags this page has not been taught about is left the plain
	// colour rather than being made to disappear.
	function edgeColour(actions)
	{
		var mixed = [0, 0, 0];
		var known = 0;

		for(var index = 0; index < actions.length; index++)
		{
			var colour = ACTION_COLOURS[actions[index]];

			if(!colour) continue;

			mixed[0] += colour[0];
			mixed[1] += colour[1];
			mixed[2] += colour[2];

			known++;
		}

		if(known === 0) return EDGE_COLOUR;

		return [mixed[0] / known, mixed[1] / known, mixed[2] / known];
	}

	// Lifts a colour towards white, which is how the thing under the pointer is picked out from everything
	// else without changing what colour it is understood to be.
	function highlighted(colour)
	{
		return [
			colour[0] + (1 - colour[0]) * 0.55,
			colour[1] + (1 - colour[1]) * 0.55,
			colour[2] + (1 - colour[2]) * 0.55
		];
	}

	// -- Layout --

	// Repulsion is what stops nodes piling up, the spring is what holds joined nodes near each other, and the
	// pull to the origin is what keeps a part of the graph that is joined to nothing from drifting off. The
	// damping is what makes the whole thing come to rest rather than oscillate for ever.
	var LAYOUT_REPULSION = 3.2;
	var LAYOUT_SPRING = 0.09;
	var LAYOUT_REST_LENGTH = 2.3;
	var LAYOUT_ORIGIN_PULL = 0.012;
	var LAYOUT_DAMPING = 0.86;
	var LAYOUT_MAX_STEP = 0.4;

	/// Ticks run before the graph is first drawn, so it arrives looking like a graph rather than as a knot
	/// that untangles while being watched.
	var LAYOUT_PRESETTLE_TICKS = 220;

	/// Ticks run per drawn frame once it is on screen, and the energy at which the layout is called settled.
	var LAYOUT_TICKS_PER_FRAME = 2;
	var LAYOUT_SETTLE_ENERGY = 0.0004;
	var LAYOUT_MAX_ANIMATED_TICKS = 900;

	var nodes = [];
	var edges = [];
	var nodeById = {};

	/// Where each node is and how fast it is moving, kept against the node's id so that a graph arriving again
	/// carries on from where the last one settled instead of starting over.
	var positions = {};
	var velocities = {};

	var layoutTicksLeft = 0;

	/// Whether a graph has been drawn yet. Told apart from whether the camera has been framed, since a camera
	/// can be put back where it was left before any graph has arrived to frame it against.
	var graphLoaded = false;

	// A repeatable pseudo-random unit value for a node id, used to place a node this page has not seen before.
	// Seeded from the id rather than taken from Math.random() so that reloading a page puts the same graph in
	// the same place.
	function seededUnit(id, salt)
	{
		var value = Math.sin(id * 127.1 + salt * 311.7 + 13.0) * 43758.5453;

		return value - Math.floor(value);
	}

	// Puts a node not yet placed somewhere on a sphere big enough to hold the graph, so the layout starts from
	// something spread out rather than from every node sitting on top of every other.
	function seedPosition(id, nodeCount)
	{
		var radius = 1.5 + Math.pow(nodeCount, 1 / 3);

		var theta = seededUnit(id, 1) * Math.PI * 2;
		var cosPhi = seededUnit(id, 2) * 2 - 1;
		var sinPhi = Math.sqrt(Math.max(0, 1 - cosPhi * cosPhi));

		return [
			radius * sinPhi * Math.cos(theta),
			radius * sinPhi * Math.sin(theta),
			radius * cosPhi
		];
	}

	// Advances the layout by one step, returning how much movement there was in it. All pairs repel, which is
	// only affordable because a hive holds tens or hundreds of nodes rather than the thousands where this would
	// need dividing into cells.
	function layoutTick()
	{
		var count = nodes.length;

		if(count === 0) return 0;

		var forces = {};
		var index;

		for(index = 0; index < count; index++) forces[nodes[index].id] = [0, 0, 0];

		for(index = 0; index < count; index++)
		{
			var posnA = positions[nodes[index].id];
			var forceA = forces[nodes[index].id];

			for(var other = index + 1; other < count; other++)
			{
				var posnB = positions[nodes[other].id];

				var dx = posnA[0] - posnB[0];
				var dy = posnA[1] - posnB[1];
				var dz = posnA[2] - posnB[2];

				var distSq = dx * dx + dy * dy + dz * dz;

				// Two nodes that have ended up in the same place have no direction to be pushed apart along,
				// so they are nudged apart by their ids instead of dividing by zero.
				if(distSq < 1e-6)
				{
					dx = seededUnit(nodes[index].id, 3) - 0.5;
					dy = seededUnit(nodes[other].id, 4) - 0.5;
					dz = seededUnit(nodes[index].id, 5) - 0.5;

					distSq = dx * dx + dy * dy + dz * dz + 1e-6;
				}

				var dist = Math.sqrt(distSq);
				var push = LAYOUT_REPULSION / distSq;

				var forceB = forces[nodes[other].id];

				forceA[0] += (dx / dist) * push;
				forceA[1] += (dy / dist) * push;
				forceA[2] += (dz / dist) * push;

				forceB[0] -= (dx / dist) * push;
				forceB[1] -= (dy / dist) * push;
				forceB[2] -= (dz / dist) * push;
			}

			// Everything drifts gently back towards the origin, which both keeps the graph framed and stops
			// a group of nodes joined to nothing else being pushed away for ever.
			forceA[0] -= posnA[0] * LAYOUT_ORIGIN_PULL;
			forceA[1] -= posnA[1] * LAYOUT_ORIGIN_PULL;
			forceA[2] -= posnA[2] * LAYOUT_ORIGIN_PULL;
		}

		for(index = 0; index < edges.length; index++)
		{
			var edge = edges[index];

			var from = positions[edge.fromNodeId];
			var to = positions[edge.toNodeId];

			if(!from || !to) continue;

			var ex = to[0] - from[0];
			var ey = to[1] - from[1];
			var ez = to[2] - from[2];

			var edgeLength = Math.sqrt(ex * ex + ey * ey + ez * ez) || 1e-3;

			// Shared out between the edges running between the same two nodes, so that a pair joined more than
			// once is drawn together no harder than a pair joined once. Without this a pair pulls tighter the
			// more edges it has, which is both wrong -- where the nodes sit would say how many edges they have
			// rather than what the graph is shaped like -- and awkward to look at, since those are exactly the
			// pairs whose edges are bowed apart and so need the most room between them.
			var pull = (edgeLength - LAYOUT_REST_LENGTH) * LAYOUT_SPRING / edge.fanCount;

			var fromForce = forces[edge.fromNodeId];
			var toForce = forces[edge.toNodeId];

			fromForce[0] += (ex / edgeLength) * pull;
			fromForce[1] += (ey / edgeLength) * pull;
			fromForce[2] += (ez / edgeLength) * pull;

			toForce[0] -= (ex / edgeLength) * pull;
			toForce[1] -= (ey / edgeLength) * pull;
			toForce[2] -= (ez / edgeLength) * pull;
		}

		var energy = 0;

		for(index = 0; index < count; index++)
		{
			var id = nodes[index].id;

			var velocity = velocities[id];
			var force = forces[id];
			var posn = positions[id];

			for(var axis = 0; axis < 3; axis++)
			{
				velocity[axis] = (velocity[axis] + force[axis]) * LAYOUT_DAMPING;

				// A node that has been pushed very hard, which happens when two land almost on top of each
				// other, would otherwise be thrown clear across the graph in a single step.
				if(velocity[axis] > LAYOUT_MAX_STEP) velocity[axis] = LAYOUT_MAX_STEP;
				else if(velocity[axis] < -LAYOUT_MAX_STEP) velocity[axis] = -LAYOUT_MAX_STEP;

				posn[axis] += velocity[axis];

				energy += velocity[axis] * velocity[axis];
			}
		}

		return energy / count;
	}

	// -- Geometry --

	/// Flat arrays uploaded to the GL buffers, rebuilt whenever the layout moves or the hover changes.
	var meshPositions = [];
	var meshColours = [];
	var meshNormals = [];

	/// One entry per triangle in the arrays above, saying what that triangle belongs to, which is how a ray
	/// hit becomes the node or edge it landed on.
	var pickIds = [];

	function pushTriangle(a, b, c, normalA, normalB, normalC, colour, pickId)
	{
		meshPositions.push(a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2]);

		meshNormals.push(normalA[0], normalA[1], normalA[2], normalB[0], normalB[1], normalB[2],
			normalC[0], normalC[1], normalC[2]);

		for(var vertex = 0; vertex < 3; vertex++) meshColours.push(colour[0], colour[1], colour[2], 1);

		pickIds.push(pickId);
	}

	// Builds a sphere out of latitude/longitude bands. Kept coarse deliberately: it is redrawn on every step of
	// the layout, and at this size the difference between a coarse sphere and a fine one is not worth the
	// hundreds of extra triangles per node.
	var SPHERE_LAT_BANDS = 6;
	var SPHERE_LONG_BANDS = 10;

	function buildSphere(centre, radius, colour, pickId)
	{
		for(var lat = 0; lat < SPHERE_LAT_BANDS; lat++)
		{
			var theta0 = (lat / SPHERE_LAT_BANDS) * Math.PI;
			var theta1 = ((lat + 1) / SPHERE_LAT_BANDS) * Math.PI;

			for(var long = 0; long < SPHERE_LONG_BANDS; long++)
			{
				var phi0 = (long / SPHERE_LONG_BANDS) * Math.PI * 2;
				var phi1 = ((long + 1) / SPHERE_LONG_BANDS) * Math.PI * 2;

				var normals = [
					[Math.sin(theta0) * Math.cos(phi0), Math.cos(theta0), Math.sin(theta0) * Math.sin(phi0)],
					[Math.sin(theta1) * Math.cos(phi0), Math.cos(theta1), Math.sin(theta1) * Math.sin(phi0)],
					[Math.sin(theta1) * Math.cos(phi1), Math.cos(theta1), Math.sin(theta1) * Math.sin(phi1)],
					[Math.sin(theta0) * Math.cos(phi1), Math.cos(theta0), Math.sin(theta0) * Math.sin(phi1)]
				];

				var points = [];

				for(var corner = 0; corner < 4; corner++)
				{
					points.push([
						centre[0] + normals[corner][0] * radius,
						centre[1] + normals[corner][1] * radius,
						centre[2] + normals[corner][2] * radius
					]);
				}

				// The bands meeting at each pole are triangles rather than quads, so one of the two triangles
				// of the quad collapses to nothing there and is left out. Which one it is differs by pole: the
				// first and last corners meet at the top, the middle two at the bottom. Leaving out the wrong
				// one of the pair puts a hole at each pole, which is a hole straight through the sphere.
				if(lat < SPHERE_LAT_BANDS - 1) pushTriangle(points[0], points[1], points[2], normals[0], normals[1], normals[2], colour, pickId);
				if(lat > 0) pushTriangle(points[0], points[2], points[3], normals[0], normals[2], normals[3], colour, pickId);
			}
		}
	}

	// The six faces of a cube, as the direction each looks along and the two directions across it. Each face
	// carries its own flat normal, which is what gives a cube its hard edges rather than the shading running
	// smoothly around it the way a sphere's does.
	var CUBE_FACES = [

		{ normal: [1, 0, 0], across: [0, 1, 0], up: [0, 0, 1] },
		{ normal: [-1, 0, 0], across: [0, 0, 1], up: [0, 1, 0] },
		{ normal: [0, 1, 0], across: [0, 0, 1], up: [1, 0, 0] },
		{ normal: [0, -1, 0], across: [1, 0, 0], up: [0, 0, 1] },
		{ normal: [0, 0, 1], across: [1, 0, 0], up: [0, 1, 0] },
		{ normal: [0, 0, -1], across: [0, 1, 0], up: [1, 0, 0] }
	];

	function cubeCorner(centre, face, half, acrossSign, upSign)
	{
		return [
			centre[0] + (face.normal[0] + face.across[0] * acrossSign + face.up[0] * upSign) * half,
			centre[1] + (face.normal[1] + face.across[1] * acrossSign + face.up[1] * upSign) * half,
			centre[2] + (face.normal[2] + face.across[2] * acrossSign + face.up[2] * upSign) * half
		];
	}

	// Builds a cube of the given half width about a centre, which is how a node that puts geometry into a
	// scene is told apart from one that does not.
	function buildCube(centre, half, colour, pickId)
	{
		for(var index = 0; index < CUBE_FACES.length; index++)
		{
			var face = CUBE_FACES[index];

			var corner00 = cubeCorner(centre, face, half, -1, -1);
			var corner10 = cubeCorner(centre, face, half, 1, -1);
			var corner11 = cubeCorner(centre, face, half, 1, 1);
			var corner01 = cubeCorner(centre, face, half, -1, 1);

			pushTriangle(corner00, corner10, corner11, face.normal, face.normal, face.normal, colour, pickId);
			pushTriangle(corner00, corner11, corner01, face.normal, face.normal, face.normal, colour, pickId);
		}
	}

	// The four points of a tetrahedron, as every other corner of a cube, and the three of them that make up
	// each of its faces.
	var TETRAHEDRON_POINTS = [[1, 1, 1], [1, -1, -1], [-1, 1, -1], [-1, -1, 1]];
	var TETRAHEDRON_FACES = [[0, 1, 2], [0, 3, 1], [0, 2, 3], [1, 3, 2]];

	// Works out a tetrahedron of reach one about the origin, as a list of triangles each with its own flat
	// normal, the same way the dodecahedron below is worked out. Doing it once rather than per node also
	// leaves its faces to hand, which is what says where its surface is in any given direction.
	function buildTetrahedronTemplate()
	{
		// The corners of a cube of side two are a distance of the square root of three from its middle, so
		// this is what brings them in to a reach of one.
		var scale = 1 / Math.sqrt(3);

		var points = [];
		var index;

		for(index = 0; index < TETRAHEDRON_POINTS.length; index++)
		{
			points.push([
				TETRAHEDRON_POINTS[index][0] * scale,
				TETRAHEDRON_POINTS[index][1] * scale,
				TETRAHEDRON_POINTS[index][2] * scale
			]);
		}

		var triangles = [];

		for(index = 0; index < TETRAHEDRON_FACES.length; index++)
		{
			var a = points[TETRAHEDRON_FACES[index][0]];
			var b = points[TETRAHEDRON_FACES[index][1]];
			var c = points[TETRAHEDRON_FACES[index][2]];

			var normal = normalize3(cross3([b[0] - a[0], b[1] - a[1], b[2] - a[2]],
				[c[0] - a[0], c[1] - a[1], c[2] - a[2]]));

			// Turned outwards where the order the face's points are listed in left it facing back into the
			// solid, so that every face is lit by what is in front of it rather than by what is behind it.
			if(dot3(normal, [(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3]) < 0)
			{
				normal = [-normal[0], -normal[1], -normal[2]];
			}

			triangles.push({ a: a, b: b, c: c, normal: normal });
		}

		return triangles;
	}

	var TETRAHEDRON_TRIANGLES = buildTetrahedronTemplate();

	/// How near a tetrahedron's faces come to its middle, against how far its points reach. Taken from the
	/// shape itself rather than written down, so the two cannot drift apart.
	var TETRAHEDRON_INRADIUS = dot3(TETRAHEDRON_TRIANGLES[0].a, TETRAHEDRON_TRIANGLES[0].normal);

	// Builds a tetrahedron whose points reach the given distance from a centre. Like a cube, each face
	// carries one flat normal, so it reads as a solid with hard edges rather than as a rounded thing.
	function buildTetrahedron(centre, radius, colour, pickId)
	{
		buildFromTemplate(TETRAHEDRON_TRIANGLES, centre, radius, colour, pickId);
	}

	// Puts a worked-out shape of reach one down at a size and a place.
	function buildFromTemplate(triangles, centre, radius, colour, pickId)
	{
		for(var index = 0; index < triangles.length; index++)
		{
			var triangle = triangles[index];

			pushTriangle(
				[centre[0] + triangle.a[0] * radius, centre[1] + triangle.a[1] * radius, centre[2] + triangle.a[2] * radius],
				[centre[0] + triangle.b[0] * radius, centre[1] + triangle.b[1] * radius, centre[2] + triangle.b[2] * radius],
				[centre[0] + triangle.c[0] * radius, centre[1] + triangle.c[1] * radius, centre[2] + triangle.c[2] * radius],
				triangle.normal, triangle.normal, triangle.normal, colour, pickId);
		}
	}

	// Works out a dodecahedron of reach one about the origin, as a plain list of triangles each with its own
	// flat normal. Done once when the page loads rather than per node per frame, since which of its twenty
	// points make up each of its twelve faces is far more work than a redraw should be repeating.
	function buildDodecahedronTemplate()
	{
		var golden = (1 + Math.sqrt(5)) / 2;
		var signs = [-1, 1];

		var points = [];
		var normals = [];

		var i, j, k;

		// A dodecahedron's points are the corners of a cube together with three rectangles, one turned
		// through each axis, whose sides are in the golden ratio. Its twelve faces look along another three
		// such rectangles, turned the other way about. Getting those two the wrong way round gives twelve
		// directions that look plausible and are not faces at all, which is why the points of a face are
		// gathered below by what a face actually is rather than by counting off the nearest five.
		for(i = 0; i < 2; i++)
		{
			for(j = 0; j < 2; j++)
			{
				for(k = 0; k < 2; k++) points.push([signs[i], signs[j], signs[k]]);

				points.push([0, signs[i] / golden, signs[j] * golden]);
				points.push([signs[i] / golden, signs[j] * golden, 0]);
				points.push([signs[i] * golden, 0, signs[j] / golden]);

				normals.push(normalize3([0, signs[i] * golden, signs[j]]));
				normals.push(normalize3([signs[i] * golden, signs[j], 0]));
				normals.push(normalize3([signs[i], 0, signs[j] * golden]));
			}
		}

		// Every point of a dodecahedron built this way is the square root of three from its middle.
		var reach = Math.sqrt(3);

		for(i = 0; i < points.length; i++)
		{
			points[i] = [points[i][0] / reach, points[i][1] / reach, points[i][2] / reach];
		}

		var triangles = [];

		for(i = 0; i < normals.length; i++)
		{
			var normal = normals[i];

			// A face is where a plane touches the solid without cutting it, so its points are every point
			// lying as far along the direction it looks in as any point does. Gathered that way rather than
			// by taking a set number of them, so a direction that is not a face cannot quietly pass for one.
			var furthest = -Infinity;

			for(j = 0; j < points.length; j++) furthest = Math.max(furthest, dot3(points[j], normal));

			var face = [];

			for(j = 0; j < points.length; j++)
			{
				if(dot3(points[j], normal) > furthest - 1e-6) face.push(points[j]);
			}

			// Put in order around that direction, so that fanning them below walks the pentagon's rim rather
			// than cutting back and forth across it.
			var across = perpendicularBasis(normal);

			face.sort(function(a, b)
			{
				return Math.atan2(dot3(a, across[1]), dot3(a, across[0])) -
					Math.atan2(dot3(b, across[1]), dot3(b, across[0]));
			});

			for(j = 1; j + 1 < face.length; j++)
			{
				triangles.push({ a: face[0], b: face[j], c: face[j + 1], normal: normal });
			}
		}

		return triangles;
	}

	var DODECAHEDRON_TRIANGLES = buildDodecahedronTemplate();

	/// How near a dodecahedron's faces come to its middle, against how far its points reach. Taken from the
	/// shape itself rather than written down, so the two cannot drift apart.
	var DODECAHEDRON_INRADIUS = dot3(DODECAHEDRON_TRIANGLES[0].a, DODECAHEDRON_TRIANGLES[0].normal);

	// Builds a dodecahedron whose points reach the given distance from a centre.
	function buildDodecahedron(centre, radius, colour, pickId)
	{
		buildFromTemplate(DODECAHEDRON_TRIANGLES, centre, radius, colour, pickId);
	}

	// The way each of a shape's faces looks, one entry per face, taken from the worked-out shape itself. What
	// these are for is saying where a shape's surface is in a direction, which is what an edge meeting it has
	// to be cut back to.
	function faceNormalsOf(triangles)
	{
		var normals = [];

		for(var index = 0; index < triangles.length; index++)
		{
			var normal = triangles[index].normal;
			var known = false;

			for(var seen = 0; seen < normals.length; seen++)
			{
				if(dot3(normals[seen], normal) > 0.999999) { known = true; break; }
			}

			if(!known) normals.push(normal);
		}

		return normals;
	}

	var CUBE_NORMALS = (function()
	{
		var normals = [];

		for(var index = 0; index < CUBE_FACES.length; index++) normals.push(CUBE_FACES[index].normal);

		return normals;
	})();

	var TETRAHEDRON_NORMALS = faceNormalsOf(TETRAHEDRON_TRIANGLES);
	var DODECAHEDRON_NORMALS = faceNormalsOf(DODECAHEDRON_TRIANGLES);

	// How far it is from the middle of a flat sided shape to its surface, along a direction. The shape is
	// where every one of its faces says it is, so the surface is met at whichever face is met first.
	function planeReach(normals, inradius, direction)
	{
		var nearest = Infinity;

		for(var index = 0; index < normals.length; index++)
		{
			var facing = dot3(direction, normals[index]);

			// A face turned away from the direction being asked about is behind, not in front.
			if(facing <= 1e-6) continue;

			var reach = inradius / facing;

			if(reach < nearest) nearest = reach;
		}

		return (nearest === Infinity) ? inradius : nearest;
	}

	// Two directions at right angles to the given one, so a ring can be built around it.
	function perpendicularBasis(axis)
	{
		var reference = (Math.abs(axis[1]) < 0.9) ? [0, 1, 0] : [1, 0, 0];

		var side = normalize3(cross3(axis, reference));

		return [side, normalize3(cross3(axis, side))];
	}

	var TUBE_SIDES = 6;

	/// How far out to the side an edge is bowed when it shares its two nodes with another. Enough to clear
	/// the node spheres it runs between, so that a pair reads as two edges from any distance.
	var FAN_BOW = 0.5;

	/// How many straight pieces a bowed edge's tube is built from. A straight edge needs only the one.
	var FAN_SEGMENTS = 8;

	/// How finely a bowed edge's curve is walked to find where it leaves one node and meets the other.
	var FAN_TRIM_STEPS = 48;

	// The directions a tube's ring of corners is built out along, given the two directions across it. Worked
	// out once per edge and used for every piece of it, so that the pieces of a bowed edge line up with each
	// other instead of each being twisted its own way.
	function ringDirections(basis)
	{
		var directions = [];

		for(var side = 0; side < TUBE_SIDES; side++)
		{
			var angle = (side / TUBE_SIDES) * Math.PI * 2;

			var cos = Math.cos(angle);
			var sin = Math.sin(angle);

			directions.push([
				basis[0][0] * cos + basis[1][0] * sin,
				basis[0][1] * cos + basis[1][1] * sin,
				basis[0][2] * cos + basis[1][2] * sin
			]);
		}

		return directions;
	}

	// One piece of tube, running between two points.
	function buildTubePiece(a, b, radius, directions, colour, pickId)
	{
		for(var side = 0; side < TUBE_SIDES; side++)
		{
			var normal0 = directions[side];
			var normal1 = directions[(side + 1) % TUBE_SIDES];

			var a0 = [a[0] + normal0[0] * radius, a[1] + normal0[1] * radius, a[2] + normal0[2] * radius];
			var a1 = [a[0] + normal1[0] * radius, a[1] + normal1[1] * radius, a[2] + normal1[2] * radius];
			var b0 = [b[0] + normal0[0] * radius, b[1] + normal0[1] * radius, b[2] + normal0[2] * radius];
			var b1 = [b[0] + normal1[0] * radius, b[1] + normal1[1] * radius, b[2] + normal1[2] * radius];

			pushTriangle(a0, b0, b1, normal0, normal0, normal1, colour, pickId);
			pushTriangle(a0, b1, a1, normal0, normal1, normal1, colour, pickId);
		}
	}

	// The cone, and the disc closing the back of it so it is not hollow when looked into from behind, that
	// say which way an edge runs.
	function buildArrowHead(base, tip, axis, directions, colour, pickId)
	{
		var backwards = [-axis[0], -axis[1], -axis[2]];

		for(var side = 0; side < TUBE_SIDES; side++)
		{
			var normal0 = directions[side];
			var normal1 = directions[(side + 1) % TUBE_SIDES];

			var head0 = [base[0] + normal0[0] * ARROW_RADIUS, base[1] + normal0[1] * ARROW_RADIUS, base[2] + normal0[2] * ARROW_RADIUS];
			var head1 = [base[0] + normal1[0] * ARROW_RADIUS, base[1] + normal1[1] * ARROW_RADIUS, base[2] + normal1[2] * ARROW_RADIUS];

			pushTriangle(head0, tip, head1, normal0, axis, normal1, colour, pickId);
			pushTriangle(base, head1, head0, backwards, backwards, backwards, colour, pickId);
		}
	}

	// Builds the tube and arrowhead that stand for an edge. Both ends are pulled back to the surface of the
	// node they meet, so the tube reads as joining two nodes rather than disappearing into them, and the head
	// sits just short of the target so which way the edge runs can be seen from any angle.
	// An edge that shares its two nodes with another is bowed out to the side by the given offset, so that
	// each one can be seen and hovered separately instead of them all being drawn along the same line. The
	// bow is in the middle only: a bowed edge still leaves and arrives exactly where a straight one would.
	function buildEdge(from, to, bow, fromNode, toNode, colour, pickId)
	{
		var along = [to[0] - from[0], to[1] - from[1], to[2] - from[2]];
		var length = Math.sqrt(dot3(along, along));

		if(length < 1e-6) return;

		var axis = [along[0] / length, along[1] / length, along[2] / length];
		var back = [-axis[0], -axis[1], -axis[2]];

		// Asked of each node along the line this edge meets it on, so that an edge arriving at a corner of a
		// shape is cut back to that corner and not to the much nearer face beside it.
		var fromReach = nodeReach(fromNode, axis);
		var toReach = nodeReach(toNode, back);

		// Two nodes so close that there is no room between them for an edge, which the layout will separate
		// on its next step.
		if(length < fromReach + toReach + ARROW_LENGTH) return;

		var directions = ringDirections(perpendicularBasis(axis));

		if(bow[0] === 0 && bow[1] === 0 && bow[2] === 0)
		{
			var start = [
				from[0] + axis[0] * fromReach,
				from[1] + axis[1] * fromReach,
				from[2] + axis[2] * fromReach
			];

			var headBase = [
				to[0] - axis[0] * (toReach + ARROW_LENGTH),
				to[1] - axis[1] * (toReach + ARROW_LENGTH),
				to[2] - axis[2] * (toReach + ARROW_LENGTH)
			];

			var headTip = [
				to[0] - axis[0] * toReach,
				to[1] - axis[1] * toReach,
				to[2] - axis[2] * toReach
			];

			buildTubePiece(start, headBase, EDGE_RADIUS, directions, colour, pickId);
			buildArrowHead(headBase, headTip, axis, directions, colour, pickId);

			return;
		}

		// A curve through both node centres, pushed out sideways in the middle. Doubling the offset at the
		// control point is what makes the curve itself pass the asked-for distance from the straight line,
		// since a quadratic only reaches halfway to where its control point is.
		var control = [
			(from[0] + to[0]) / 2 + bow[0] * 2,
			(from[1] + to[1]) / 2 + bow[1] * 2,
			(from[2] + to[2]) / 2 + bow[2] * 2
		];

		function curveAt(t)
		{
			var it = 1 - t;

			return [
				it * it * from[0] + 2 * it * t * control[0] + t * t * to[0],
				it * it * from[1] + 2 * it * t * control[1] + t * t * to[1],
				it * it * from[2] + 2 * it * t * control[2] + t * t * to[2]
			];
		}

		// Where along the curve it clears the node it leaves and reaches the head of the node it meets. Walked
		// rather than solved, since the curve is drawn from the node centres and only the part between their
		// surfaces is wanted.
		var startT = 0;
		var endT = 1;
		var step;

		// Asked of the direction each point along the curve actually lies in, rather than of the straight
		// line between the nodes, since a bowed edge leaves and arrives at an angle to it.
		function outside(point, centre, node, clearance)
		{
			var away = [point[0] - centre[0], point[1] - centre[1], point[2] - centre[2]];
			var span = Math.sqrt(dot3(away, away));

			if(span < 1e-9) return false;

			return span >= nodeReach(node, [away[0] / span, away[1] / span, away[2] / span]) + clearance;
		}

		for(step = 0; step <= FAN_TRIM_STEPS; step++)
		{
			startT = step / FAN_TRIM_STEPS;

			if(outside(curveAt(startT), from, fromNode, 0)) break;
		}

		for(step = FAN_TRIM_STEPS; step >= 0; step--)
		{
			endT = step / FAN_TRIM_STEPS;

			if(outside(curveAt(endT), to, toNode, ARROW_LENGTH)) break;
		}

		if(!(startT < endT)) return;

		var previous = curveAt(startT);

		for(step = 1; step <= FAN_SEGMENTS; step++)
		{
			var next = curveAt(startT + (endT - startT) * (step / FAN_SEGMENTS));

			buildTubePiece(previous, next, EDGE_RADIUS, directions, colour, pickId);

			previous = next;
		}

		// The head is aimed from where the curve ends straight at the node it belongs to, so that it points
		// at what it is arriving at rather than along the straight line the curve was bowed away from.
		var headTowards = normalize3([to[0] - previous[0], to[1] - previous[1], to[2] - previous[2]]);

		var headReach = nodeReach(toNode, [-headTowards[0], -headTowards[1], -headTowards[2]]);

		var arrowTip = [
			to[0] - headTowards[0] * headReach,
			to[1] - headTowards[1] * headReach,
			to[2] - headTowards[2] * headReach
		];

		var arrowBase = [
			arrowTip[0] - headTowards[0] * ARROW_LENGTH,
			arrowTip[1] - headTowards[1] * ARROW_LENGTH,
			arrowTip[2] - headTowards[2] * ARROW_LENGTH
		];

		var headDirections = ringDirections(perpendicularBasis(headTowards));

		// Closes whatever the walk above left between the end of the curve and the back of the head.
		buildTubePiece(previous, arrowBase, EDGE_RADIUS, headDirections, colour, pickId);

		buildArrowHead(arrowBase, arrowTip, headTowards, headDirections, colour, pickId);
	}

	// Where an edge that shares its two nodes with others is pushed out to. Worked out about the line running
	// from the lower node id to the higher one, whichever way round the edge itself runs, so that the two
	// edges of a mutual pair are pushed to different places rather than each working it out from its own
	// direction and both landing in the same one. Spread around that line rather than all bowed the same way,
	// so that three or more do not collapse back into one line when looked at from the side.
	function fanBow(fromNodeId, toNodeId, from, to, fanIndex, fanCount)
	{
		if(fanCount < 2) return [0, 0, 0];

		var low = (fromNodeId < toNodeId) ? from : to;
		var high = (fromNodeId < toNodeId) ? to : from;

		var along = [high[0] - low[0], high[1] - low[1], high[2] - low[2]];
		var length = Math.sqrt(dot3(along, along));

		if(length < 1e-6) return [0, 0, 0];

		var basis = perpendicularBasis([along[0] / length, along[1] / length, along[2] / length]);

		var angle = (fanIndex / fanCount) * Math.PI * 2;

		var cos = Math.cos(angle);
		var sin = Math.sin(angle);

		return [
			(basis[0][0] * cos + basis[1][0] * sin) * FAN_BOW,
			(basis[0][1] * cos + basis[1][1] * sin) * FAN_BOW,
			(basis[0][2] * cos + basis[1][2] * sin) * FAN_BOW
		];
	}

	// -- Hover --

	/// What the pointer is currently over, as one of the pick entries built with the geometry, or null.
	var hovered = null;

	// Finds what is under a screen point, as { id: <the pick entry built with the geometry>, point: <where in
	// the world the ray met it> }, or null when the ray met nothing. The point is what lets a right click
	// centre on the part of an edge that was pressed rather than on one of its ends.
	function pickAt(clientX, clientY)
	{
		if(!lastViewProj || pickIds.length === 0) return null;

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
		var closest = null;

		for(var triIndex = 0; triIndex < pickIds.length; triIndex++)
		{
			var base = triIndex * 9;

			var v0 = [meshPositions[base], meshPositions[base + 1], meshPositions[base + 2]];
			var v1 = [meshPositions[base + 3], meshPositions[base + 4], meshPositions[base + 5]];
			var v2 = [meshPositions[base + 6], meshPositions[base + 7], meshPositions[base + 8]];

			var hitT = rayTriangleIntersect(origin, dir, v0, v1, v2);

			if(hitT !== null && hitT < closestT)
			{
				closestT = hitT;
				closest = pickIds[triIndex];
			}
		}

		if(closest === null) return null;

		return {

			id: closest,
			point: [origin[0] + dir[0] * closestT, origin[1] + dir[1] * closestT, origin[2] + dir[2] * closestT]
		};
	}

	// Everything the page has to say about a node or an edge is said here, in HTML over the canvas, rather
	// than drawn into the scene. Written with textContent throughout: a node's name is whatever the hive it
	// came from called it, and must never be treated as markup.
	function showTooltip(pick, clientX, clientY)
	{
		if(pick.kind === 'node')
		{
			var node = nodeById[pick.nodeId];

			if(!node) return;

			tooltipTitle.textContent = node.name;
			tooltipBody.textContent = nodeTypeLabel(node.type);
		}
		else
		{
			var edge = edges[pick.edgeIndex];
			var fromNode = nodeById[edge.fromNodeId];
			var toNode = nodeById[edge.toNodeId];

			tooltipTitle.textContent = (fromNode ? fromNode.name : edge.fromNodeId) + ' → ' +
				(toNode ? toNode.name : edge.toNodeId);

			// An edge carrying no action flags restricts nothing, so every action may traverse it. That is the
			// opposite of what it looks like, which is why it is said rather than left as an empty list.
			tooltipBody.textContent = edge.actions.length > 0 ? edge.actions.join('\n') : 'Unrestricted';
		}

		tooltip.hidden = false;

		// Placed clear of the cursor, and flipped back over it rather than being allowed off the edge of the
		// window, where it could not be read.
		var left = clientX + 14;
		var top = clientY + 14;

		if(left + tooltip.offsetWidth > window.innerWidth) left = clientX - tooltip.offsetWidth - 14;
		if(top + tooltip.offsetHeight > window.innerHeight) top = clientY - tooltip.offsetHeight - 14;

		tooltip.style.left = Math.max(0, left) + 'px';
		tooltip.style.top = Math.max(0, top) + 'px';
	}

	function hideTooltip()
	{
		tooltip.hidden = true;
	}

	// Puts the called-out node's panel where that node is on the screen, which has to be worked out afresh
	// every frame: the node is still moving while the layout settles, and it moves again whenever the camera
	// does. Sat above the node rather than beside it, clear of however big that node is drawn.
	function updateCallout()
	{
		if(!callout) return;

		var node = (calloutNodeId === null) ? null : nodeById[calloutNodeId];
		var posn = (calloutNodeId === null) ? null : positions[calloutNodeId];

		if(!node || !posn || !lastViewProj)
		{
			callout.hidden = true;

			return;
		}

		var clip = transformVec4(lastViewProj, posn[0], posn[1], posn[2], 1);

		// Behind the camera, where there is no sensible place on the screen to put it.
		if(clip[3] <= 0)
		{
			callout.hidden = true;

			return;
		}

		var rect = canvas.getBoundingClientRect();

		var x = rect.left + ((clip[0] / clip[3]) * 0.5 + 0.5) * rect.width;
		var y = rect.top + (0.5 - (clip[1] / clip[3]) * 0.5) * rect.height;

		// How tall the node itself is on the screen, so the panel clears a well connected one as surely as it
		// clears a leaf. Measured by projecting a point one reach above the node rather than assumed.
		var upwards = nodeReach(node, calloutUp);

		var above = transformVec4(lastViewProj, posn[0] + calloutUp[0] * upwards,
			posn[1] + calloutUp[1] * upwards, posn[2] + calloutUp[2] * upwards, 1);

		var gap = 10;

		if(above[3] > 0) gap += Math.abs(y - (rect.top + (0.5 - (above[1] / above[3]) * 0.5) * rect.height));

		callout.hidden = false;

		var left = x - callout.offsetWidth / 2;
		var top = y - callout.offsetHeight - gap;

		callout.style.left = Math.max(0, Math.min(window.innerWidth - callout.offsetWidth, left)) + 'px';
		callout.style.top = Math.max(0, Math.min(window.innerHeight - callout.offsetHeight, top)) + 'px';
	}

	// Stops pointing a node out. Leaves redrawing to the caller, which is generally about to anyway.
	function clearCallout()
	{
		calloutNodeId = null;

		if(callout) callout.hidden = true;
	}

	function samePick(a, b)
	{
		if(a === null || b === null) return a === b;
		if(a.kind !== b.kind) return false;

		return (a.kind === 'node') ? a.nodeId === b.nodeId : a.edgeIndex === b.edgeIndex;
	}

	function updateHover(clientX, clientY)
	{
		var hit = pickAt(clientX, clientY);
		var pick = (hit !== null) ? hit.id : null;

		// Bringing the pointer onto the node this page was sent here to point out means it has been found,
		// so the panel saying where it is stops being worth the room it takes up over the scene.
		var found = pick !== null && pick.kind === 'node' && pick.nodeId === calloutNodeId;

		if(found) clearCallout();

		if(!samePick(pick, hovered) || found)
		{
			hovered = pick;

			// The highlight is part of the geometry's colours, so what is under the pointer only costs a
			// rebuild when it actually changes rather than on every movement of the mouse.
			rebuildGeometry();
		}

		if(pick === null)
		{
			canvas.style.cursor = 'default';
			hideTooltip();

			return;
		}

		canvas.style.cursor = 'pointer';

		showTooltip(pick, clientX, clientY);
	}

	// -- Camera state, driven by mouse drag (orbit), right drag (pan) and wheel (zoom) --

	var camera = { yaw: 0.6, pitch: 0.4, distance: 20, centre: [0, 0, 0] };

	var CAMERA_FOVY = Math.PI / 4;
	var CENTRE_ANIM_MS = 400;

	/// Fraction of the viewport height the whole graph is made to span when it is first framed. Measured
	/// against the graph's widest axis rather than the diagonal of its bounds, since fitting the diagonal
	/// leaves a graph sitting in the middle of a mostly empty window. A little short of the whole height, so
	/// that an orbit which brings two long axes across the screen together still has somewhere to put them.
	var FIT_VIEWPORT_FRACTION = 0.75;

	var cameraFitted = false;
	var jumpedToCallout = false;

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

	var lastViewProj = null;

	/// Which way is up on the screen, in the world, kept from the last frame drawn so that a panel can be put
	/// above what it belongs to rather than merely near it.
	var calloutUp = [0, 1, 0];

	var dragging = false;
	var panning = false;
	var mouseDownOnCanvas = false;
	var lastX = 0, lastY = 0;
	var mouseDownX = 0, mouseDownY = 0;
	var panLastX = 0, panLastY = 0;
	var rightDownX = 0, rightDownY = 0;
	var rightDragMoved = false;

	var centreAnimFrom = null;
	var centreAnimTo = null;
	var centreAnimStartTime = 0;

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

		// The pointer is about to be dragged, and a panel left sitting over the scene while the camera moves
		// under it describes whatever it was over when the drag started.
		hideTooltip();
	});

	window.addEventListener('mouseup', function(e)
	{
		dragging = false;
		panning = false;

		if(!mouseDownOnCanvas) return;

		mouseDownOnCanvas = false;

		var moveDist = Math.hypot(e.clientX - mouseDownX, e.clientY - mouseDownY);

		// Real pointing devices (especially trackpads) commonly drift several pixels between mousedown and
		// mouseup even on a deliberate click, so this needs to be forgiving. Nothing here is clicked, so a
		// release that was not a drag simply picks the hover back up where it left off.
		if(moveDist < 4) updateHover(e.clientX, e.clientY);
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
			// Only track hover while the camera isn't being manipulated, so an orbit/pan drag doesn't leave a
			// panel following the pointer describing whatever it happens to cross.
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
	// to naturally clear it.
	canvas.addEventListener('mouseleave', function(e)
	{
		canvas.style.cursor = 'default';

		hideTooltip();

		if(hovered !== null)
		{
			hovered = null;
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

	// Right-clicking a node or an edge recentres the orbit on it instead of opening the browser's context
	// menu. Distance/yaw/pitch are left alone so this reads as a recentre, not a jump-cut.
	canvas.addEventListener('contextmenu', function(e)
	{
		e.preventDefault();

		// A right drag pans (handled in mousemove above); only a stationary right-click recentres, so a
		// pan doesn't also snap the centre onto whatever happened to be under the release point.
		if(rightDragMoved) return;

		var hit = pickAt(e.clientX, e.clientY);

		if(hit === null) return;

		// A node centres on the node itself rather than on the bit of its surface that was pressed, so that
		// it ends up in the middle of the view and stays there as the camera is orbited around it. An edge
		// has no such middle, so it centres on the point along it that was pressed.
		var target = (hit.id.kind === 'node' && positions[hit.id.nodeId]) ?
			positions[hit.id.nodeId].slice() : hit.point;

		// Start from wherever the centre currently is, which is the tween's midpoint if a previous
		// recentre is still in flight, so a fast double pick doesn't stutter back to the old target.
		centreAnimFrom = camera.centre.slice();
		centreAnimTo = target;
		centreAnimStartTime = performance.now();

		scheduleDraw();
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

	// Builds the whole scene from where the layout currently has the nodes, and uploads it. Everything is
	// rebuilt rather than only what moved, because a step of the layout moves every node at once and the
	// graphs a hive holds are small enough for that to cost less than tracking what changed would.
	function rebuildGeometry()
	{
		meshPositions = [];
		meshColours = [];
		meshNormals = [];
		pickIds = [];

		var index;

		for(index = 0; index < edges.length; index++)
		{
			var edge = edges[index];

			var from = positions[edge.fromNodeId];
			var to = positions[edge.toNodeId];

			if(!from || !to) continue;

			var bow = fanBow(edge.fromNodeId, edge.toNodeId, from, to, edge.fanIndex, edge.fanCount);

			// Lifted towards white when hovered, the same way a node is, rather than being swapped for a
			// colour of its own: an edge under the pointer should still say what it lets through.
			var colour = edgeColour(edge.actions);

			if(hovered !== null && hovered.kind === 'edge' && hovered.edgeIndex === index)
			{
				colour = highlighted(colour);
			}

			// Each end is trimmed to the node it meets, since the shapes do not all take up the same room.
			var fromNode = nodeById[edge.fromNodeId];
			var toNode = nodeById[edge.toNodeId];

			buildEdge(from, to, bow, fromNode, toNode, colour, { kind: 'edge', edgeIndex: index });
		}

		for(index = 0; index < nodes.length; index++)
		{
			var node = nodes[index];
			var posn = positions[node.id];

			if(!posn) continue;

			var colour = nodeColour(node.type);

			// A node being pointed out is lit up as though the pointer were on it, which is what makes it
			// findable at a glance rather than only by reading the panel above it.
			if((hovered !== null && hovered.kind === 'node' && hovered.nodeId === node.id) ||
				node.id === calloutNodeId)
			{
				colour = highlighted(colour);
			}

			var pickId = { kind: 'node', nodeId: node.id };

			var scale = nodeScale(node);

			if(CUBE_NODE_TYPES[node.type]) buildCube(posn, NODE_RADIUS * scale, colour, pickId);
			else if(TETRAHEDRON_NODE_TYPES[node.type]) buildTetrahedron(posn, TETRAHEDRON_RADIUS * scale, colour, pickId);
			else if(DODECAHEDRON_NODE_TYPES[node.type]) buildDodecahedron(posn, DODECAHEDRON_RADIUS * scale, colour, pickId);
			else buildSphere(posn, NODE_RADIUS * scale, colour, pickId);
		}

		vertexCount = meshPositions.length / 3;

		gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(meshPositions), gl.DYNAMIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(meshColours), gl.DYNAMIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(meshNormals), gl.DYNAMIC_DRAW);

		scheduleDraw();
	}

	// Frames the whole graph once, the first time there is one to frame. Only done once so that a graph
	// changing later does not drag the camera back from wherever it has since been put.
	function fitCamera()
	{
		if(cameraFitted || nodes.length === 0) return;

		var min = [Infinity, Infinity, Infinity];
		var max = [-Infinity, -Infinity, -Infinity];

		for(var index = 0; index < nodes.length; index++)
		{
			var posn = positions[nodes[index].id];

			if(!posn) continue;

			for(var axis = 0; axis < 3; axis++)
			{
				if(posn[axis] < min[axis]) min[axis] = posn[axis];
				if(posn[axis] > max[axis]) max[axis] = posn[axis];
			}
		}

		if(min[0] === Infinity) return;

		camera.centre = [(min[0] + max[0]) / 2, (min[1] + max[1]) / 2, (min[2] + max[2]) / 2];

		var radius = 0;

		for(var boundsAxis = 0; boundsAxis < 3; boundsAxis++)
		{
			var half = (max[boundsAxis] - min[boundsAxis]) / 2;

			if(half > radius) radius = half;
		}

		radius += NODE_RADIUS;

		camera.distance = Math.max(2, radius / (FIT_VIEWPORT_FRACTION * Math.tan(CAMERA_FOVY / 2)));

		cameraFitted = true;
	}

	// Says what the shapes and colours on the canvas stand for. Built from the types the graph actually holds
	// rather than from every type there is, so it describes what is on the screen instead of listing kinds of
	// node that are not there. Written as HTML over the canvas, like everything else this page says in words.
	function rebuildLegend()
	{
		if(!legend) return;

		while(legend.firstChild) legend.removeChild(legend.firstChild);

		var present = {};
		var index;

		for(index = 0; index < nodes.length; index++) present[nodes[index].type] = true;

		// Listed in the order the colours are declared in rather than the order the graph happens to mention
		// them, so that the legend does not reshuffle itself when the graph changes. Anything present that has
		// no colour of its own follows on the end, drawn the way it is drawn: as a plain node.
		var types = [];
		var type;

		for(type in NODE_COLOURS) if(present[type]) types.push(type);

		for(type in present) if(!NODE_COLOURS.hasOwnProperty(type)) types.push(type);

		for(index = 0; index < types.length; index++)
		{
			var colour = nodeColour(types[index]);

			var swatch = document.createElement('span');

			swatch.className = 'legendSwatch ' + (CUBE_NODE_TYPES[types[index]] ? 'legendCube' :
				(TETRAHEDRON_NODE_TYPES[types[index]] ? 'legendTetrahedron' :
				(DODECAHEDRON_NODE_TYPES[types[index]] ? 'legendDodecahedron' : 'legendSphere')));

			swatch.style.background = 'rgb(' + Math.round(colour[0] * 255) + ',' +
				Math.round(colour[1] * 255) + ',' + Math.round(colour[2] * 255) + ')';

			var label = document.createElement('span');

			// A type name is text rather than markup, the same as everything else taken from the graph.
			label.textContent = nodeTypeLabel(types[index]);

			var row = document.createElement('div');

			row.className = 'legendRow';

			row.appendChild(swatch);
			row.appendChild(label);

			legend.appendChild(row);
		}

		legend.hidden = types.length === 0;
	}

	function describeGraph()
	{
		if(nodes.length === 0) return 'This hive holds no nodes.';

		return nodes.length + (nodes.length === 1 ? ' node, ' : ' nodes, ') +
			edges.length + (edges.length === 1 ? ' edge' : ' edges') +
			'. Hover for detail, drag to orbit, scroll to zoom, right-drag to pan, right-click to centre.';
	}

	function loadGraph(data)
	{
		nodes = data.nodes || [];
		nodeById = {};
		edges = [];

		var index;

		for(index = 0; index < nodes.length; index++) nodeById[nodes[index].id] = nodes[index];

		// Flattened out of the nodes, because an edge is drawn and hovered as a thing in its own right rather
		// than as part of the node it leads from. An edge leading to a node that is not in the graph is left
		// out; there is nowhere to draw it to.
		for(index = 0; index < nodes.length; index++)
		{
			var nodeEdges = nodes[index].edges || [];

			for(var edgeIndex = 0; edgeIndex < nodeEdges.length; edgeIndex++)
			{
				if(!nodeById.hasOwnProperty(nodeEdges[edgeIndex].toNodeId)) continue;

				edges.push({

					fromNodeId: nodes[index].id,
					toNodeId: nodeEdges[edgeIndex].toNodeId,
					actions: nodeEdges[edgeIndex].actions || [],
					fanIndex: 0,
					fanCount: 1
				});
			}
		}

		// Count what meets each node, so that it can be drawn to a size that says how much of the graph runs
		// through it. Both ends of an edge count, since an edge meets a node whichever way it runs.
		for(index = 0; index < nodes.length; index++) nodes[index].degree = 0;

		for(index = 0; index < edges.length; index++)
		{
			nodeById[edges[index].fromNodeId].degree++;
			nodeById[edges[index].toNodeId].degree++;
		}

		// Work out which edges share their two nodes with another, so that each can be bowed to a place of its
		// own. Grouped without regard to which way round they run, since an edge each way between a pair sits
		// on the same line as a pair running the same way does. Numbered in the order the hive catalogued them,
		// which does not change from one poll to the next, so an edge stays where it was put.
		var fanGroups = {};

		for(index = 0; index < edges.length; index++)
		{
			var edge = edges[index];

			var key = Math.min(edge.fromNodeId, edge.toNodeId) + ':' + Math.max(edge.fromNodeId, edge.toNodeId);

			if(!fanGroups.hasOwnProperty(key)) fanGroups[key] = [];

			edge.fanIndex = fanGroups[key].length;

			fanGroups[key].push(edge);
		}

		for(var groupKey in fanGroups)
		{
			var group = fanGroups[groupKey];

			for(var member = 0; member < group.length; member++) group[member].fanCount = group.length;
		}

		// Anything the pointer was over may not be there any more, and an edge is named by its position in a
		// list that has just been rebuilt.
		hovered = null;
		hideTooltip();

		var seen = {};

		for(index = 0; index < nodes.length; index++)
		{
			var id = nodes[index].id;

			seen[id] = true;

			// A node already placed stays where it is, so a graph that has only gained or lost a node
			// settles from where it was rather than being laid out afresh.
			if(!positions[id])
			{
				positions[id] = seedPosition(id, nodes.length);
				velocities[id] = [0, 0, 0];
			}
		}

		for(var knownId in positions)
		{
			if(!seen[knownId])
			{
				delete positions[knownId];
				delete velocities[knownId];
			}
		}

		// Run hard before the first frame so the graph arrives looking like a graph, then left to animate to
		// a stop so that a change to an already settled layout is seen to move rather than jumping. Asked of
		// whether a graph has been drawn before rather than of the camera, which may have been put where it
		// was last left before any graph arrived at all.
		var presettleTicks = graphLoaded ? 0 : LAYOUT_PRESETTLE_TICKS;

		for(index = 0; index < presettleTicks; index++) layoutTick();

		layoutTicksLeft = LAYOUT_MAX_ANIMATED_TICKS;

		fitCamera();

		// Sent here to point a node out. The camera is put on it once, rather than every time the graph is
		// polled, so that it is not dragged back off whatever has since been looked at instead.
		if(calloutNodeId !== null && nodeById[calloutNodeId] && positions[calloutNodeId])
		{
			if(callout)
			{
				calloutTitle.textContent = nodeById[calloutNodeId].name;
				calloutBody.textContent = nodeTypeLabel(nodeById[calloutNodeId].type);
			}

			if(!jumpedToCallout)
			{
				camera.centre = positions[calloutNodeId].slice();

				jumpedToCallout = true;
			}
		}
		else if(calloutNodeId !== null)
		{
			// Named a node this hive has not got, or has since lost.
			clearCallout();
		}

		rebuildGeometry();
		rebuildLegend();

		graphLoaded = true;

		status.textContent = describeGraph();
	}

	// True while a requestAnimationFrame(draw) call is outstanding, so scheduleDraw() can be called freely
	// from every input handler and state change without ever stacking up more than one pending frame.
	var rafPending = false;

	// Requests a single redraw. draw() only reschedules itself while the layout is still settling or the
	// recentre tween is in flight, so the render loop otherwise goes idle between input events instead of
	// spinning at the display refresh rate while nothing is moving.
	function scheduleDraw()
	{
		if(rafPending) return;

		rafPending = true;
		requestAnimationFrame(draw);
	}

	function draw()
	{
		rafPending = false;

		var layoutMoving = false;

		if(layoutTicksLeft > 0)
		{
			var energy = 0;

			for(var tick = 0; tick < LAYOUT_TICKS_PER_FRAME && layoutTicksLeft > 0; tick++)
			{
				energy = layoutTick();

				layoutTicksLeft--;
			}

			// Below this the nodes are moving by less than can be seen, so the layout is called done rather
			// than left creeping for the rest of its tick budget.
			if(energy < LAYOUT_SETTLE_ENERGY) layoutTicksLeft = 0;

			layoutMoving = layoutTicksLeft > 0;

			rebuildGeometry();
		}

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

			// The same turn taken the other way gives which way is up on the screen, which is where a panel
			// belonging to a node has to sit.
			calloutUp = transformNormal(invViewRot, 0, 1, 0);

			gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
			gl.vertexAttribPointer(aPosition, 3, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aPosition);

			gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
			gl.vertexAttribPointer(aColor, 4, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aColor);

			gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
			gl.vertexAttribPointer(aNormal, 3, gl.FLOAT, false, 0, 0);
			gl.enableVertexAttribArray(aNormal);

			gl.disable(gl.BLEND);
			gl.depthMask(true);
			gl.drawArrays(gl.TRIANGLES, 0, vertexCount);
		}

		// Done after drawing, so it follows whatever the frame just moved: the node while the layout settles,
		// and the whole scene whenever the camera is turned.
		updateCallout();

		// Keep rendering every frame while the layout is still settling or the recentre tween is in flight;
		// otherwise leave the loop idle until the next scheduleDraw() call from an input handler or a graph
		// arriving.
		if(layoutMoving || centreAnimTo !== null) scheduleDraw();
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

			console.log('Graph revision changed: ' + lastRevision + ' -> ' + revisionData.revision);

			lastRevision = revisionData.revision;

			// The whole graph is asked for rather than a difference against what is already here. It is ids,
			// names and flags, and this only happens at all when the surface has reported a change.
			return fetch(dataUrl).then(function(res) { return res.json(); }).then(function(data)
			{
				loadGraph(data);
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
