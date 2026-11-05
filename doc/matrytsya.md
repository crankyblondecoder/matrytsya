# matrytsya Command Line Reference

`matrytsya` is the production server. It loads one or more hives, each from a JSON hive definition file, into
a single collection. It runs them and serves every hive's surfaces through a web interface on one port. It can
also run the hives against an agentic harness, so that the chat window and the agent nodes in a hive have a
model behind them.

## Synopsis

```
matrytsya [-p PORT] [-H HARNESS] [-c LEVEL] [-n] [-t COUNT] [-v] HIVE [HIVE ...]
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
At least one is required, and any number may be given.

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
| `-H`  | `--harness`               | `FILE`   | none    | Agentic harness definition to run every hive against. See [Agentic harness](#agentic-harness). |
| `-c`  | `--chat-capability`       | `LEVEL`  | `LOW`   | Capability the chat window asks for: `LOW`, `MEDIUM` or `HIGH`. Case sensitive. See [Chat window](#chat-window). |
| `-n`  | `--no-chat`               | none     | off     | Disable the chat window. See [Chat window](#chat-window). |
| `-t`  | `--threads`               | `COUNT`  | `2`     | Number of threads in each hive's thread pool. 1 to 1024. Every hive gets its own pool of this size. |
| `-v`  | `--verbose`               | none     | off     | Log model requests and responses to the console. |
| `-h`  | `--help`                  | none     |         | Print a usage summary to standard output and exit with status 0. |

Long options take their argument either as the next word (`--port 9000`) or after `=` (`--port=9000`).
Options and hive files can come in any order. Use `--` to end the options if a hive file's name starts
with `-`.

## Agentic harness

A harness is a JSON document satisfying [harnessSchema.json](harnessSchema.json). It names the model
servers, and the models, system prompts and tool sets assigned to each role and capability. It isn't part of
a hive's definition: one harness file serves every hive given on the command line.

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
Listening on http://localhost:PORT/
```

| Path                         | Serves |
|------------------------------|--------|
| `/`                          | Index of every hive loaded, with links to each hive's surfaces. Paths no surface claims also land here. |
| `/HIVE/SURFACE/`             | The page for one surface of one hive, where `HIVE` is the hive's name and `SURFACE` is the surface's name. |

- Scene surfaces are drawn as an interactive WebGL scene. Clicking or hovering over an object pokes the node
  that drew it.
- Graph view surfaces are drawn as the hive's node and edge structure.
- If a hive has a graph view surface, each of its scene pages has a **Structure** link to it.
- A surface of any other kind isn't served and doesn't appear in the index. If no hive has a surface that
  can be served, the program exits (see below).
- Pages poll the server every 50 ms for changes to what they draw.

A hive's scene doesn't start populating until the first surface page is opened in a browser. Strobing and
animation don't depend on this: they run on each hive's own threads from the moment it loads.

## Running and stopping

The program runs in the foreground until it gets `SIGINT` (Ctrl-C) or `SIGTERM`. It then stops the web server,
shuts every hive down and exits with status 0.

## Exit status and errors

| Status | When |
|--------|------|
| `0`    | Clean shutdown after `SIGINT`/`SIGTERM`, or `-h`. |
| `1`    | Any of the errors below. |

Each error is printed to standard error. All of them happen before the server starts:

| Message | Cause |
|---------|-------|
| `invalid port '...'`, `invalid chat capability '...'`, `invalid thread count '...'`, an unrecognised option, or a missing option argument | Bad command line. The usage summary follows. |
| `no hive definition files given` | No `HIVE` argument. The usage summary follows. |
| `Could not open hive definition file: PATH` | The hive file doesn't exist or can't be read. |
| `Could not load the hive described by PATH: DESCRIPTION` | The hive file isn't valid JSON, or doesn't describe a valid hive. `DESCRIPTION` says what's wrong, e.g. `the file is not valid JSON -- line 3, column 17: Invalid value.` or `a surface names a node that does not exist`. |
| `The hive described by PATH is named '...', which another hive already loaded is named too.` | Two hive files share a name. |
| `None of the hive definition files loaded produced a surface to serve.` | No hive has a scene or graph view surface. |
| `Could not start HTTP server on port PORT (error N)` | The port is in use, or the program isn't allowed to bind it. |

Harness problems are also printed to standard error, but they aren't fatal (see
[Agentic harness](#agentic-harness)). The message says what went wrong, followed by
`Running every hive with no model behind it.`:

| Message | Cause |
|---------|-------|
| `Could not open harness definition file: PATH` | The harness file doesn't exist or can't be read. |
| `Could not build the harness described by PATH: could not get the list of models from a model provider -- provider 'NAME' at URL: could not connect` | Nothing is listening at the provider's URL, or it can't be reached from this machine. Start the server, or correct the `url` in the harness file. |
| `... provider 'NAME' at URL: connected, but the list of models it returned could not be read` | Something answered at the URL, but not with a model list. Usually the URL points at the wrong server or port. |
| `Could not build the harness described by PATH: a model assignment names a model its provider does not serve -- model 'MODEL' on provider 'NAME'` | The provider is up but doesn't have that model. For Ollama, pull it on the server (`ollama pull MODEL`). |
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

Serve one hive with the harness driving its agent nodes, but with no chat window in the browser:

```
./matrytsya --no-chat --harness=../examples/ollamaHarness.json ../examples/engineHive.json
```
