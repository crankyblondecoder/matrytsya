# matrytsya Command Line Reference

`matrytsya` is the production server. It loads one or more hives, each from a JSON hive definition file or
from a hive database, into a single collection. It runs them and serves every hive's surfaces through a web
interface on one port. It can also run the hives against an agentic harness, so that the chat window and the
agent nodes in a hive have a model behind them, and keep the hives stored in a hive database as they change.

## Synopsis

```
matrytsya [-p PORT] [-a ADDRESS] [-H HARNESS] [-c LEVEL] [-n] [-t COUNT] [-v] HIVE [HIVE ...]
matrytsya -d DATABASE [-p PORT] [-a ADDRESS] [-H HARNESS] [-c LEVEL] [-n] [-t COUNT] [-v] [HIVE ...]
matrytsya -h
```

## Building

From the `build` directory:

| Command        | Produces                 |
|----------------|--------------------------|
| `make release` | `build/matrytsya`        |
| `make debug`   | `build/matrytsya_debug`  |

Both build from `src/matrytsya.cpp` and take the same command line.

## Arguments

`HIVE` is the path of a hive definition file: a JSON document satisfying [hiveSchema.json](hiveSchema.json).
At least one is required, and any number may be given. With `-d`, none is required as long as the database
already holds a hive (see [Hive database](#hive-database)).

- Relative paths are resolved against the current working directory, not the executable's location.
- All the hives go into one collection, so a teleport node in one hive can forward an action to a node in
  another hive loaded in the same run.
- Each hive's `name` must be distinct across the files given. The name is also the hive's path in the web
  interface.
- Hives are loaded in the order given. The first file that can't be opened or doesn't describe a valid hive
  stops the program before the server starts (see [Exit status and errors](#exit-status-and-errors)).

## Options

| Short | Long                      | Argument | Default | Meaning |
|-------|---------------------------|----------|---------|---------|
| `-p`  | `--port`                  | `PORT`   | `8080`  | Port the web interface listens on. 1 to 65535. |
| `-a`  | `--address`               | `ADDRESS` | `127.0.0.1` | IPv4 address of the network interface the web interface listens on. The default only accepts connections from this machine. Use `0.0.0.0` to listen on every interface, or a specific interface's address to listen on just that one. See [Web interface](#web-interface). |
| `-d`  | `--database`              | `DATABASE` | none  | Hive database to load hives from and keep them stored in. Created if it doesn't exist. See [Hive database](#hive-database). |
| `-H`  | `--harness`               | `FILE`   | none    | Agentic harness definition to run every hive against. See [Agentic harness](#agentic-harness). |
| `-c`  | `--chat-capability`       | `LEVEL`  | `LOW`   | Capability the chat window asks for: `LOW`, `MEDIUM` or `HIGH`. Case sensitive. See [Chat window](#chat-window). |
| `-n`  | `--no-chat`               | none     | off     | Disable the chat window. See [Chat window](#chat-window). |
| `-s`  | `--shading`               | `MODEL`  | `BLINN_PHONG` | Lighting model the WebGL scene and structure views shade with: `BLINN_PHONG` (diffuse plus a specular highlight, worked out per pixel) or `LAMBERT` (diffuse only, worked out per vertex). Case sensitive. |
| `-t`  | `--threads`               | `COUNT`  | `2`     | Number of threads in each hive's thread pool. 1 to 1024. Every hive gets its own pool of this size. |
| `-v`  | `--verbose`               | none     | off     | Raise the console logging level by one. Can be repeated. See [Verbosity](#verbosity). |
| `-h`  | `--help`                  | none     |         | Print a usage summary to standard output and exit with status 0. |

Long options take their argument either as the next word (`--port 9000`) or after `=` (`--port=9000`).
Options and hive files can come in any order. Use `--` to end the options if a hive file's name starts
with `-`.

## Verbosity

Each `-v` raises the logging level by one. Each level logs everything the level below it logs, plus more:

| Level | Given as            | Logs to the console |
|-------|---------------------|---------------------|
| 0     | no `-v`             | Startup and error messages only. |
| 1     | `-v`                | Adds model requests and responses. |
| 2     | `-vv` or `-v -v`    | Adds every poke a node accepts, eg `Node with id:2 was poked with a HIT poke.` |

Levels above 2 log the same as level 2.

## Agentic harness

A harness is a JSON document satisfying [harnessSchema.json](harnessSchema.json). It names the model
servers, and the models, system prompts and tool sets assigned to each role and capability. It isn't part of
a hive's definition: one harness file serves every hive given on the command line.

- Two kinds of provider can serve models. `Ollama` is a server at a given `url`. `OpenRouter` is reached over
  HTTPS at `https://openrouter.ai/api/v1` unless `url` says otherwise. It reads its API key from the
  environment variable named by `apiKeyEnv`, so the key never sits in the harness file. HTTPS servers are
  verified against the system's certificate authorities in `/etc/ssl/certs/ca-certificates.crt`.
- Without `-H`, the hives run with no model behind them. Everything else still works. The chat window
  reports that there's no model to chat with, and agent nodes can't get their requests answered.
- A separate harness is built from the file for each hive, because the tools it gives a model report on
  that one hive. So every server the file names is contacted once per hive at start-up, and start-up blocks
  until they answer.
- If the harness file can't be opened, isn't valid, or names a server or model that can't be reached, the
  problem is printed once and every hive runs with no model behind it. Once the harness has failed for one
  hive, it isn't tried again for the rest. Unlike a bad hive file, this doesn't stop the program. See
  [Exit status and errors](#exit-status-and-errors) for what each message means.
- [modelTools.md](modelTools.md) lists the tools a model is given at each node type, and which role and
  capability a harness has to assign for a hive's requests to be answered.

## Chat window

Every surface page carries a chat window by default. Through it a user can talk with a model about the hive
the surface belongs to. The model sees that hive through its chat tools.

- `-c LEVEL` sets which capability the chat window asks the harness for. The harness must assign a model to
  the `CHAT` role at that capability, or chat requests fail with an error shown in the window. For example,
  `examples/ollamaHarness.json` assigns `CHAT` at `LOW`, which is the default here.
- `-n` / `--no-chat` turns the chat window off on every surface page:
  - The window and the button that opens it aren't shown. The page's other controls move into the space
    they leave.
  - The chat endpoints under each surface's path (`.../chat`, `.../chat/...`) answer `404` with
    `{"error":"chat is disabled"}`. A request made by hand can't reach a model either.
  - A harness given with `-H` is still built and still answers the hive's agent nodes. Only chat from the
    browser is disabled.
  - `-c` has no effect.

## Web interface

Once started, the server prints:

```
Listening on http://ADDRESS:PORT/
```

or, with `-a 0.0.0.0`:

```
Listening on every interface, port PORT, e.g. http://localhost:PORT/
```

The web interface has no authentication. Anyone who can reach it can drive the hives and, with a harness,
the models behind them. Only listen beyond loopback on a network you trust.

| Path                         | Serves |
|------------------------------|--------|
| `/`                          | Index of every hive loaded, with links to each hive's surfaces. Paths no surface claims also land here. |
| `/HIVE/SURFACE/`             | The page for one surface of one hive, where `HIVE` is the hive's name and `SURFACE` is the surface's name. |

- If only one hive is given, `/` sends the browser on to that hive's first scene surface, or to its only
  surface if it has no scene surface, or to its page listing the surfaces otherwise.
- Scene surfaces are drawn as an interactive WebGL scene. Clicking or hovering over an object pokes the node
  that drew it.
- Graph view surfaces are drawn as the hive's node and edge structure.
- If a hive has a graph view surface, each of its scene pages has a **Structure** link to it.
- Each scene page shows in its top right corner how many browser pages currently have that scene surface
  open. A page stops being counted once it's closed, or once it has gone a few seconds without polling.
- A surface of any other kind isn't served and doesn't appear in the index. If no hive has a surface that
  can be served, the program exits (see below).
- Pages poll the server every 50 ms for changes to what they draw.

A hive's scene doesn't start populating until the first surface page is opened in a browser. Strobing and
animation don't depend on this: they run on each hive's own threads from the moment it loads.

## Hive database

A hive can grow while it runs: a builder script node creates nodes and edges between them, and can register
the scene roots it creates as strobe emitters. Without a hive database, all of that is lost when the program
stops, and the hive is back as its hive file describes it on the next run. With `-d DATABASE`, every hive is
kept stored in the database as it changes, and is loaded from it as it was left on the next run.

The database is a single [LMDB](https://symas.com/lmdb/) file at `DATABASE`, with LMDB's lock file beside it
at `DATABASE-lock`. Both are created if they don't exist. Don't run two programs against the same database at
once: each keeps its own hives stored, and would overwrite what the other stores of any hive they share.

### Which hives are loaded

- Every hive the database holds is loaded first, in name order.
- Each `HIVE` file is then read. If the database already holds a hive of the same `name`, the stored hive is
  used, the file is ignored, and a line saying so is printed. Otherwise the hive is loaded from the file and
  stored in the database straight away.
- To start a hive afresh from its file, use a new database, or rename the hive in its file.

### What is stored

- **Each node as it joined the hive.** A node loaded from a hive file is stored as the file describes it.
  A node a builder creates is stored as it was created. Changes a script makes to a node afterwards, such as a
  transform being animated or text being changed, aren't stored, so the next run starts the hive's scenes
  where they started rather than mid-animation.
- **Every edge, as it stands.** That includes edges a builder adds to nodes that were loaded from a file.
- **Whether each builder has used up its one build.** A builder that has built is loaded back as having
  built, so it doesn't build the same nodes again on top of the stored ones.
- **Strobe emitter and strobe surface registrations, as they stand.**
- **Surfaces,** which don't change while a hive runs.

### When it is written

A change is written about half a second after the hive stops changing, as one transaction holding only the
records that changed. A hive that keeps changing is written at least every five seconds. Anything not yet
written is written when the program stops. A change made in the last moment before the program is killed
without warning (for example with `SIGKILL`) can be lost; earlier changes are not.

A failure to write while running is printed to standard error as
`[ERROR] Could not store hive 'NAME': DESCRIPTION`. The hive keeps running, and the write is retried.

### Inspecting a database

Each hive is a named LMDB database, named after the hive. Its records are keyed `meta`, `node/NAME`,
`surface/NAME`, `strobeEmitter/NAME` and `strobeSurface/NAME`. Each value is the JSON object that the same
thing is written as in a hive file. The LMDB command line tools show them, e.g. `mdb_dump -p -s HIVENAME
-n DATABASE`, or `mdb_stat -a -n DATABASE` to list the hives held.

## Running and stopping

The program runs in the foreground until it gets `SIGINT` (Ctrl-C) or `SIGTERM`. It then stops the web server,
writes anything not yet stored to the hive database if there is one, shuts every hive down and exits with
status 0.

## Exit status and errors

| Status | When |
|--------|------|
| `0`    | Clean shutdown after `SIGINT`/`SIGTERM`, or `-h`. |
| `1`    | Any of the errors below. |

Each error is printed to standard error. All of them happen before the server starts:

| Message | Cause |
|---------|-------|
| `invalid port '...'`, `invalid address '...'`, `invalid chat capability '...'`, `invalid thread count '...'`, an unrecognised option, or a missing option argument | Bad command line. The usage summary follows. |
| `no hive definition files given` | No `HIVE` argument. The usage summary follows. |
| `no hive definition files given, and DATABASE holds no hives` | No `HIVE` argument, and the hive database is empty. The usage summary follows. |
| `Could not open hive database DATABASE: DESCRIPTION` | The database file can't be created or opened, e.g. its directory doesn't exist, or it isn't an LMDB file. |
| `Could not load the hive 'NAME' stored in DATABASE: DESCRIPTION` | A stored hive can't be read back, e.g. a record is missing or malformed, or it was written by a newer version of this program. |
| `Could not store the hive ... in DATABASE: DESCRIPTION` | A hive loaded from a file couldn't be written to the database. |
| `Could not open hive definition file: PATH` | The hive file doesn't exist or can't be read. |
| `Could not load the hive described by PATH: DESCRIPTION` | The hive file isn't valid JSON, or doesn't describe a valid hive. `DESCRIPTION` says what's wrong, e.g. `the file is not valid JSON -- line 3, column 17: Invalid value.` or `a surface names a node that does not exist`. |
| `The hive described by PATH is named '...', which another hive already loaded is named too.` | Two hive files share a name. |
| `None of the hive definition files loaded produced a surface to serve.` | No hive has a scene or graph view surface. |
| `Could not start HTTP server on ADDRESS:PORT (error N)` | The port is in use, the program isn't allowed to bind it, or `ADDRESS` isn't an address of this machine. |

Harness problems are also printed to standard error, but they aren't fatal (see
[Agentic harness](#agentic-harness)). The message says what went wrong, followed by
`Running every hive with no model behind it.`:

| Message | Cause |
|---------|-------|
| `Could not open harness definition file: PATH` | The harness file doesn't exist or can't be read. |
| `Could not build the harness described by PATH: could not get the list of models from a model provider -- provider 'NAME' at URL: could not connect` | Nothing is listening at the provider's URL, or it can't be reached from this machine. Start the server, or correct the `url` in the harness file. |
| `... provider 'NAME' at URL: connected, but the list of models it returned could not be read` | Something answered at the URL, but not with a model list. Usually the URL points at the wrong server or port. |
| `... provider 'NAME' at URL: connected, but the API key was rejected` | OpenRouter turned the key away. Check the key held in the environment variable named by the provider's `apiKeyEnv`. |
| `Could not build the harness described by PATH: the environment variable a model provider reads its API key from is not set -- provider 'NAME' reads it from 'VARIABLE'` | An OpenRouter provider's `apiKeyEnv` names a variable that is unset or empty. Export the key under that name before starting. |
| `Could not build the harness described by PATH: a model assignment names a model its provider does not serve -- model 'MODEL' on provider 'NAME'` | The provider is up but doesn't have that model. For Ollama, pull it on the server (`ollama pull MODEL`). For OpenRouter, use the model's full id as listed on openrouter.ai, e.g. `qwen/qwen3-coder`. |
| `Could not build the harness described by PATH: DESCRIPTION` | Any other problem with the harness file, described the same way as hive file problems above. |

A connection failure may be preceded by a line such as `mongoose.c:...:mg_error ... socket error`. That's
the network library logging the same failure.

## Examples

Serve the two example hives on the default port, with no model behind them. Run this from the `build`
directory:

```
./matrytsya ../examples/engineHive.json ../examples/flowerHive.json
```

Serve the same hives on port 9000, run against the example Ollama harness, and log model traffic:

```
./matrytsya -p 9000 -H ../examples/ollamaHarness.json -v ../examples/engineHive.json ../examples/flowerHive.json
```

Serve the same hives against the example OpenRouter harness instead, which reads the API key from
`OPENROUTER_API_KEY`:

```
OPENROUTER_API_KEY=sk-or-... ./matrytsya -H ../examples/openRouterHarness.json ../examples/engineHive.json ../examples/flowerHive.json
```

Serve one hive with the harness driving its agent nodes, but with no chat window in the browser:

```
./matrytsya --no-chat --harness=../examples/ollamaHarness.json ../examples/engineHive.json
```

Serve the example hives to other machines on the network, listening on every interface:

```
./matrytsya -a 0.0.0.0 ../examples/engineHive.json ../examples/flowerHive.json
```

Serve the seed hive, keeping it stored in `seed.mdb` as its builder grows it:

```
./matrytsya -d seed.mdb ../examples/seedHive.json
```

Serve it again later, as it was left, from the database alone:

```
./matrytsya -d seed.mdb
```
