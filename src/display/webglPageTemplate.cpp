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

	// -- Camera state, driven by mouse drag (orbit) and wheel (zoom) --

	var camera = { yaw: 0.6, pitch: 0.4, distance: 10, centre: [0, 0, 0] };
	var dragging = false;
	var lastX = 0, lastY = 0;

	// Only auto-fit the camera to the scene once: it's driven off each load's bounding box, and re-fitting on
	// every subsequent load (e.g. from an in-progress animation) would fight the user's own zoom/orbit input.
	var cameraFitted = false;

	canvas.addEventListener('mousedown', function(e) { dragging = true; lastX = e.clientX; lastY = e.clientY; });
	window.addEventListener('mouseup', function() { dragging = false; });
	window.addEventListener('mousemove', function(e)
	{
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

	// -- Shaders --

	var vertexShaderSrc =
		'attribute vec3 aPosition;' +
		'attribute vec4 aColor;' +
		'attribute vec3 aNormal;' +
		'uniform mat4 uViewProj;' +
		'varying vec4 vColor;' +
		'void main() {' +
		'  vec3 lightDir = normalize(vec3(0.5, 0.7, 1.0));' +
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

		var minX = Infinity, minY = Infinity, minZ = Infinity;
		var maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;

		for(var c = 0; c < data.chunks.length; c++)
		{
			var chunk = data.chunks[c];
			var modelTransform = data.modelTransforms[chunk.modelTransformIndex].transform;

			for(var v = 0; v < chunk.vertexes.length; v++)
			{
				var vertex = chunk.vertexes[v];

				var worldPos = transformPoint(modelTransform, vertex.posn[0], vertex.posn[1], vertex.posn[2]);
				var worldNormal = transformNormal(modelTransform, vertex.normal[0], vertex.normal[1], vertex.normal[2]);

				positions.push(worldPos[0], worldPos[1], worldPos[2]);
				colors.push(vertex.colour[0] / 255, vertex.colour[1] / 255, vertex.colour[2] / 255,
					vertex.colour[3] / 255);
				normals.push(worldNormal[0], worldNormal[1], worldNormal[2]);

				minX = Math.min(minX, worldPos[0]); maxX = Math.max(maxX, worldPos[0]);
				minY = Math.min(minY, worldPos[1]); maxY = Math.max(maxY, worldPos[1]);
				minZ = Math.min(minZ, worldPos[2]); maxZ = Math.max(maxZ, worldPos[2]);
			}
		}

		vertexCount = positions.length / 3;

		gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, colorBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(colors), gl.STATIC_DRAW);

		gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(normals), gl.STATIC_DRAW);

		if(vertexCount > 0 && !cameraFitted)
		{
			camera.centre = [(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2];

			var radius = Math.max(0.001, Math.sqrt(
				Math.pow(maxX - minX, 2) + Math.pow(maxY - minY, 2) + Math.pow(maxZ - minZ, 2)) / 2);

			camera.distance = radius * 2.5;

			cameraFitted = true;
		}

		status.textContent = vertexCount + ' vertexes across ' + data.chunks.length + ' chunk(s). Drag to orbit, scroll to zoom.';
	}

	function draw()
	{
		gl.clearColor(0.125, 0.125, 0.125, 1);
		gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
		gl.enable(gl.DEPTH_TEST);

		if(vertexCount > 0)
		{
			var view = multiply(translate(0, 0, -camera.distance),
				multiply(rotateX(camera.pitch),
				multiply(rotateY(camera.yaw),
				translate(-camera.centre[0], -camera.centre[1], -camera.centre[2]))));

			var proj = perspective(Math.PI / 4, canvas.width / canvas.height, 0.01, camera.distance * 100 + 100);

			var viewProj = multiply(proj, view);

			gl.uniformMatrix4fv(uViewProj, false, new Float32Array(viewProj));

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
			status.textContent = 'Failed to load scene data: ' + err;
		});
	}

	poll();
	setInterval(poll, %POLL_INTERVAL_MS%);
})();
</script>
</body>
</html>
)HTMLPAGE";
