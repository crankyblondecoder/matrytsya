# Hive Control API

The hive control API lets a program, a script or an AI agent fully control the hives a running `matrytsya`
server holds. Through it you can:

- **manage whole hives:** list them, create one from a hive definition, export one as a definition, and delete one;
- **build and change a hive:** add, inspect, edit and remove nodes, and add and remove the edges between them;
- **drive a hive:** poke, trigger and animate nodes, and set how often strobe emitters and surfaces fire.

Everything the API changes is kept in the hive database, when the server has one, just as changes a hive makes
to itself are.

This page explains the API for people. The exact contract, with every field, status and example, is
[hiveApi.openapi.json](hiveApi.openapi.json) (OpenAPI 3.1). A condensed guide for AI agents is
[hiveApiAgentGuide.md](hiveApiAgentGuide.md). The server serves all three itself, so a client needs nothing but
the server's address and a token.

## Contents

- [Setting up](#setting-up)
- [Making requests](#making-requests)
- [Concepts](#concepts)
- [Endpoints](#endpoints)
- [Errors](#errors)
- [Security](#security)
- [Limits and caveats](#limits-and-caveats)

## Setting up

The API is off unless the server is started with `-k`, naming an environment variable that holds the API token.

1. Make a token of at least 32 characters:

   ```
   export MATRYTSYA_API_TOKEN=$(openssl rand -hex 32)
   ```

2. Get a TLS certificate and key in PEM form. Unless the server only listens on loopback, it refuses to serve the
   API without one. A self-signed certificate is enough for testing:

   ```
   openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem -days 30 \
   	-subj /CN=localhost -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
   ```

3. Start the server:

   ```
   ./matrytsya -d hives.mdb -k MATRYTSYA_API_TOKEN --tls-cert cert.pem --tls-key key.pem ../examples/seedHive.json
   ```

   It prints where the API is:

   ```
   Listening on https://127.0.0.1:8080/
   Hive control API at https://127.0.0.1:8080/api/v1/
   ```

With `-k`, hive files are optional: the server can start with no hives and have them all created through the API.
See [matrytsya.md](matrytsya.md#hive-control-api) for the command line in full.

## Making requests

- Base URL: `https://HOST:PORT/api/v1`.
- Every request carries the token: `Authorization: Bearer TOKEN`.
- Every request body is a JSON object, sent with `Content-Type: application/json`.
- Every response body is JSON, including errors.
- Hive, node and surface names in paths are URL encoded.

The examples below use this shell helper, run where `cert.pem` is:

```
api() { curl -s --cacert cert.pem -H "Authorization: Bearer $MATRYTSYA_API_TOKEN" \
	-H "Content-Type: application/json" "$@"; }
B=https://127.0.0.1:8080/api/v1
```

## Concepts

The API works in the same terms as hive definition files. See [matrytsya_human.md](matrytsya_human.md) for
what they mean, and [hiveSchema.json](hiveSchema.json) for their exact form.

| Concept | In the API |
|---|---|
| **Hive** | `/hives/{hive}`. Created from a complete hive definition, exactly as in a hive file. |
| **Node** | `/hives/{hive}/nodes/{node}`. Sent and returned as one element of a definition's `nodes` array. |
| **Edge** | Belongs to the node it leaves from. Listed with its node, each with an `id`; added and removed under `/hives/{hive}/nodes/{node}/edges`. |
| **Action flags** | Named as in hive files, e.g. `PING_GRAPH_ACTION`. They restrict which actions may cross an edge. |
| **Strobe emitter** | A scene root node firing strobes on a period: `/hives/{hive}/strobeEmitters/{node}`. |
| **Strobe surface** | A surface published to its viewers on a period: `/hives/{hive}/strobeSurfaces/{surface}`. |
| **Version** | A number every hive carries that rises with each structural change, whoever makes it. Compare two readings to see if anything changed. |

### What a node's definition is

A node's `definition`, as `getNode` returns it and `getHiveDefinition` exports it, is the node **as it was when
it joined the hive**, with its **edges as they are now**. This is the same record the hive database keeps.
Anything a script has changed since, such as an animated transform, is not part of it.

### Editing rebuilds the node

A node cannot be changed in place, so an edit (`PATCH`) builds a new node from the edited definition and swaps
it in under the same name. Every edge to and from the node, and its strobe emitter registration, carries over.
The new node:

- gets a new `id`, and its edges get new ids;
- starts with no runtime state, as if just loaded.

## Endpoints

Paths are relative to `/api/v1`. The OpenAPI description gives every field, status and example.

### Discovery

| Method | Path | Does |
|---|---|---|
| GET | `/` | API version, links to the two documents below, and the agent guide's text. |
| GET | `/openapi.json` | The OpenAPI description. |
| GET | `/hiveSchema.json` | The hive definition schema, which the OpenAPI description refers to. |

### Hives

| Method | Path | Does |
|---|---|---|
| GET | `/hives` | List running hives, with version and node count. |
| POST | `/hives` | Create and start a hive from a full definition. `?persist=false` to keep it out of the database. |
| GET | `/hives/{hive}` | Summary: version, node count, whether stored, surface paths, strobe registrations. |
| GET | `/hives/{hive}/definition` | The complete hive definition, as it now stands. |
| DELETE | `/hives/{hive}` | Stop the hive. `?purge=true` also deletes it from the database. |

```
api $B/hives
api -X POST $B/hives --data-binary @myHive.json
api $B/hives/Seed/definition > seedNow.json
api -X DELETE "$B/hives/Garden?purge=true"
```

A hive is created with its surfaces served straight away; the response lists their paths. If the database
holds a hive of the same name that is not running, creating it is refused rather than overwriting the stored
one: purge that one first, or create with `?persist=false`.

Deleting without `purge` keeps the stored copy, so the server loads the hive again next time it starts. Purge
also works on a hive that is stored but not running.

### Nodes

| Method | Path | Does |
|---|---|---|
| GET | `/hives/{hive}/nodes` | Every node, with type and outgoing edges. |
| POST | `/hives/{hive}/nodes` | Add a node. |
| GET | `/hives/{hive}/nodes/{node}` | One node: definition and edges with ids. |
| PATCH | `/hives/{hive}/nodes/{node}` | Replace some of a node's fields. |
| DELETE | `/hives/{hive}/nodes/{node}` | Remove a node and every edge to and from it. |

```
api -X POST $B/hives/Seed/nodes -d '{"type": "PingNode", "name": "pinger",
	"edges": [{"toNodeName": "root", "actionFlags": ["PING_GRAPH_ACTION"]}]}'

api -X PATCH $B/hives/Seed/nodes/pinger -d '{"pokeEnabled": true}'

api -X DELETE $B/hives/Seed/nodes/pinger
```

- **Adding:** the body is a node exactly as in a hive file. Its edges must lead to nodes already in the hive, or
  to itself. To make an existing node lead to the new one, add that edge afterwards.
- **Editing:** each top level field in the body replaces that field of the node's definition; others are kept.
  Giving `edges` replaces all of the node's outgoing edges. `name` and `type` cannot be changed.

### Edges

| Method | Path | Does |
|---|---|---|
| POST | `/hives/{hive}/nodes/{node}/edges` | Add an edge leaving this node. |
| DELETE | `/hives/{hive}/nodes/{node}/edges/{edgeId}` | Remove an edge, by its id. |

```
api -X POST $B/hives/Seed/nodes/pinger/edges -d '{"to": "cube", "actionFlags": ["PING_GRAPH_ACTION"]}'
api -X DELETE $B/hives/Seed/nodes/pinger/edges/42
```

Only `to` is required. `actionFlags` (default none: any action), `traversalKey` (default 0: no key) and
`actionsCompleteAfterTraverse` (default false) work as in hive files. Adding the same edge twice makes two edges.

### Control

| Method | Path | Does |
|---|---|---|
| POST | `/hives/{hive}/nodes/{node}/poke` | Poke the node, as a viewer clicking or hovering would. |
| POST | `/hives/{hive}/nodes/{node}/trigger` | Emit a trigger action from the node. |
| POST | `/hives/{hive}/nodes/{node}/animate` | Start or stop an animating script node. |
| PUT | `/hives/{hive}/strobeEmitters/{node}` | Make a scene root a strobe emitter, or change its period. |
| DELETE | `/hives/{hive}/strobeEmitters/{node}` | Stop a node strobing. |
| PUT | `/hives/{hive}/strobeSurfaces/{surface}` | Strobe a surface, or change its period. |
| DELETE | `/hives/{hive}/strobeSurfaces/{surface}` | Stop strobing a surface. |

```
api -X POST $B/hives/Seed/nodes/pinger/poke -d '{"type": "hit"}'
api -X POST $B/hives/Seed/nodes/pinger/trigger -d '{"nodeType": "BUILDER_SCRIPT_NODE"}'
api -X POST $B/hives/Seed/nodes/cubeSpin/animate -d '{"animating": false}'
api -X PUT $B/hives/Seed/strobeEmitters/root -d '{"periodMs": 40}'
```

- **Poke** types are `hit` (the default), `grab`, `drag`, `hoverEnter` and `hoverLeave`. A node ignores pokes
  unless its `pokeEnabled` is true.
- **Trigger** sends a trigger action out along the node's edges, exactly as a model's `emitTrigger` tool does.
  It is never applied to the node it starts from. `nodeName` and `nodeType` restrict which nodes it triggers.
- Poke and trigger answer `202` straight away; what they set off happens after.

## Errors

A failed request always answers with:

```
{"error": {"code": "NOT_FOUND", "message": "No node named \"pinger\" is in this hive."}}
```

| Code | Status | Meaning |
|---|---|---|
| `UNAUTHORISED` | 401 | No token, or a wrong one. |
| `RATE_LIMITED` | 429 | Too many failed attempts from this address. Wait `Retry-After` seconds. |
| `NOT_FOUND` | 404 | No such hive, node, surface or edge. |
| `METHOD_NOT_ALLOWED` | 405 | The path does not take this method; `Allow` lists those it does. |
| `BAD_REQUEST` | 400 | The body is missing or not a JSON object. |
| `UNSUPPORTED_MEDIA_TYPE` | 415 | The body was not sent as `application/json`. |
| `CONFLICT` | 409 | Clashes with the hive's state: a name in use, a node a surface is bound to, the hive's last node, or a hive stored under the same name. |
| `UNPROCESSABLE` | 422 | The body describes something invalid. The message says what, e.g. `a node's "type" is not a known node type -- Nope`. |
| `INTERNAL` | 500 | The server failed. Details go to its log, not the response. |

## Security

- **Tokens** are compared only as SHA-256 digests, in constant time, and are removed from the server's
  environment once read. Several comma-separated tokens can be accepted at once, so a token can be rotated
  without downtime: start with `old,new`, move clients over, restart with `new`.
- **HTTPS** is required unless the server listens only on loopback. With TLS on, the whole server, viewer pages
  included, is HTTPS.
- **Lockout:** ten failed authentications from one address within a minute lock that address out for the rest
  of the minute, valid token or not.
- **Browsers:** responses carry no CORS headers, so a web page on another site cannot call the API, and the
  token is never a cookie, so a browser never sends it unasked. Responses are marked `no-store`.
- **Request size:** bodies over 1 MiB are refused before they are read.
- **The viewer is still open.** The web interface outside `/api/` has no authentication, as before. Anyone who
  can reach the port can view hives, poke them and chat. Keep that in mind before listening beyond loopback.

## Limits and caveats

- A node holds at most 32 outgoing edges. A hive must keep at least one node.
- A node that a surface is bound to (a scene surface's root or initial focus node) cannot be edited or removed
  while the hive runs. Export the definition, change it, and recreate the hive instead.
- Surfaces cannot be added or removed on a running hive; recreate the hive to change them.
- Only a `SceneRootNode` can be a strobe emitter. `VERSION_GRAPH_ACTION` cannot be named on an edge.
- Requests are handled one at a time, on the same thread as the viewer pages. Creating a hive with a harness
  contacts the model servers the harness names, which can hold up other requests while it does.
- A hive's own scripts, such as a builder, can change it between your requests. Check `version` when it matters.
