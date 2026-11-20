# Hive control API: guide for AI agents

> Dense reference for an agent driving a Matrytsya server over HTTP. The full contract is the OpenAPI
> description at `/api/v1/openapi.json`; node and hive shapes are the hive schema at `/api/v1/hiveSchema.json`.
> What hives, nodes, edges and actions mean is in `matrytsya_ai.md`.

## Basics

- Base path: `/api/v1`. Every path below is relative to it.
- Auth: send `Authorization: Bearer <token>` on every request. Ten failed attempts from one address within a
  minute lock that address out until the minute is up (429, `Retry-After`). Never guess or loop on tokens.
- Bodies: JSON objects only, sent with `Content-Type: application/json`. Responses are always JSON.
- Errors: always `{"error": {"code": "...", "message": "..."}}`. Act on `code`; show `message` to people.
- Names in paths (hive, node, surface) are URL encoded.
- Requests are handled one at a time. A hive's own scripts (e.g. a builder) may change it between your requests.
- Every structural change bumps the hive's `version` (from `getHive` / `listHives`). Compare versions to see
  whether anything changed since you last read.

## Shapes you send

- A whole hive (`createHive`): a hive definition, as in hive files. Must satisfy `hiveSchema.json`.
- One node (`createNode`): one element of a hive definition's `nodes` array: `type`, `name`, type-specific
  fields, optional `edges` (`toNodeName`, `actionFlags`, `traversalKey`, `actionsCompleteAfterTraverse`).
- A node edit (`updateNode`): only the top level fields to replace, e.g. `{"pokeEnabled": true}` or
  `{"coreScript": {...}}`. Not `name` or `type`.
- An edge (`createEdge`): `{"to": "<node>", "actionFlags": [...], "traversalKey": 0,
  "actionsCompleteAfterTraverse": false}`. Only `to` is required.

## Workflows

Inspect a hive:
1. `GET /hives` and pick the hive.
2. `GET /hives/{hive}` for its version, surfaces (with browser paths) and strobe registrations.
3. `GET /hives/{hive}/nodes` for every node, its type and outgoing edges (with edge ids).
4. `GET /hives/{hive}/nodes/{node}` for one node's full definition, or `GET /hives/{hive}/definition` for all.

Build a subgraph in a running hive:
1. Create the nodes that only lead to existing nodes first (`POST /hives/{hive}/nodes`, edges inline).
2. Then create the nodes leading to those.
3. Add edges from existing nodes into the new ones with `POST /hives/{hive}/nodes/{node}/edges`.
4. If a new scene root is to drive scripts, `PUT /hives/{hive}/strobeEmitters/{node}` with `periodMs`.
5. Re-read with `GET /hives/{hive}/nodes` to confirm.

Create a new hive:
1. Write the full definition; validate it against `hiveSchema.json` first.
2. `POST /hives` (add `?persist=false` for a throwaway hive).
3. Give people the surface paths from the response; they are browser URLs on the same server.

Edit a node:
1. `GET /hives/{hive}/nodes/{node}` and read `definition`.
2. `PATCH` only the fields that change. To change its outgoing edges, send the complete new `edges` array.
3. The node is rebuilt: it gets a new `id`, new edge ids, and loses runtime state. Edges into it are kept.

Exercise behaviour:
1. `POST .../nodes/{node}/trigger` (optionally `nodeName` / `nodeType` to restrict what it triggers), or
   `.../poke`, or `.../animate`.
2. These run after the response (202). Wait briefly, then re-read state (`version`, nodes) to see the effect.

Take something down:
- Edge: `DELETE /hives/{hive}/nodes/{node}/edges/{edgeId}`.
- Node: `DELETE /hives/{hive}/nodes/{node}` (also removes every edge into it).
- Hive: `DELETE /hives/{hive}`; add `?purge=true` to also erase it from the database. Without purge the
  server reloads it on restart.

## Safe to repeat?

| Operation | Repeat-safe | Notes |
|---|---|---|
| All `GET` | yes | |
| `setStrobeEmitter`, `setStrobeSurface` (PUT) | yes | Same period, same result. |
| `clearStrobeEmitter`, `clearStrobeSurface` (DELETE) | yes | |
| `createNode`, `createHive` | effectively | A repeat gets 409; treat as already done after checking the existing one matches. |
| `createEdge` | **no** | Each call adds another edge. List edges first. |
| `deleteNode`, `deleteHive`, `deleteEdge` | effectively | A repeat gets 404; treat as already done. |
| `updateNode` | yes | Same body, same definition (new ids each time). |
| `pokeNode`, `triggerNode` | **no** | Each call acts again. |
| `animateNode` | yes | |

Edge ids are unique and never reused, but change when their node is edited. Read them again before deleting by id.

## Error codes and what to do

| Code | HTTP | Do |
|---|---|---|
| `UNAUTHORISED` | 401 | Stop. Report that the token is missing or wrong. Do not retry with variations. |
| `RATE_LIMITED` | 429 | Wait `Retry-After` seconds, then retry once with the correct token. |
| `NOT_FOUND` | 404 | Re-list hives/nodes; the name or id is wrong or the thing was removed. |
| `METHOD_NOT_ALLOWED` | 405 | Use a method from the `Allow` header. |
| `BAD_REQUEST` | 400 | Send a JSON object body. |
| `UNSUPPORTED_MEDIA_TYPE` | 415 | Add `Content-Type: application/json`. |
| `CONFLICT` | 409 | Read current state. Name taken: pick another or use the existing one. Surface-bound node or last node: cannot be changed this way; edit the hive definition and recreate the hive instead. |
| `UNPROCESSABLE` | 422 | The message names the invalid field or value; fix the body against `hiveSchema.json`. |
| `INTERNAL` | 500 | Retry once; if it persists, stop and report. |

## Limits

- A node holds at most 32 outgoing edges.
- A hive must keep at least one node.
- Nodes a surface is bound to (a scene surface's root or initial focus node) cannot be edited or deleted while
  the hive runs.
- Only a `SceneRootNode` can be a strobe emitter.
- `VERSION_GRAPH_ACTION` cannot be named on an edge.
- Request bodies are limited to 1 MiB.

## Operations

| operationId | Method | Path | Does |
|---|---|---|---|
| `getIndex` | GET | `/` | API version, links, this guide. |
| `getOpenApi` | GET | `/openapi.json` | Full OpenAPI description. |
| `getHiveSchema` | GET | `/hiveSchema.json` | Hive definition JSON Schema. |
| `listHives` | GET | `/hives` | Running hives with versions and node counts. |
| `createHive` | POST | `/hives` | Build and start a hive from a full definition. `?persist=false` to not store it. |
| `getHive` | GET | `/hives/{hive}` | Version, node count, surfaces, strobe registrations. |
| `deleteHive` | DELETE | `/hives/{hive}` | Stop a hive. `?purge=true` also erases it from the database. |
| `getHiveDefinition` | GET | `/hives/{hive}/definition` | Full hive definition as it now stands. |
| `listNodes` | GET | `/hives/{hive}/nodes` | Every node with type and outgoing edges. |
| `createNode` | POST | `/hives/{hive}/nodes` | Add a node (definition as in a hive file). |
| `getNode` | GET | `/hives/{hive}/nodes/{node}` | One node's definition and edges with ids. |
| `updateNode` | PATCH | `/hives/{hive}/nodes/{node}` | Replace some fields; node is rebuilt in place. |
| `deleteNode` | DELETE | `/hives/{hive}/nodes/{node}` | Remove a node and all edges to and from it. |
| `createEdge` | POST | `/hives/{hive}/nodes/{node}/edges` | Add an edge from this node. |
| `deleteEdge` | DELETE | `/hives/{hive}/nodes/{node}/edges/{edgeId}` | Remove an edge by id. |
| `pokeNode` | POST | `/hives/{hive}/nodes/{node}/poke` | Poke as a viewer would. |
| `triggerNode` | POST | `/hives/{hive}/nodes/{node}/trigger` | Emit a trigger action from this node. |
| `animateNode` | POST | `/hives/{hive}/nodes/{node}/animate` | Start or stop an animating script node. |
| `setStrobeEmitter` | PUT | `/hives/{hive}/strobeEmitters/{node}` | Strobe a scene root every `periodMs`. |
| `clearStrobeEmitter` | DELETE | `/hives/{hive}/strobeEmitters/{node}` | Stop strobing a node. |
| `setStrobeSurface` | PUT | `/hives/{hive}/strobeSurfaces/{surface}` | Publish a surface every `periodMs`. |
| `clearStrobeSurface` | DELETE | `/hives/{hive}/strobeSurfaces/{surface}` | Stop publishing a surface. |
