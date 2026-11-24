#!/bin/bash

export MATRYTSYA_API_TOKEN=b640a77890aa1f946f5995043e7ef14b60e3c230c33996ffa60fae1174b15def

# The agents in the hive need a model, served here by the Ollama server named in the harness. If that server
# does not answer, the hive runs without a harness: the scene still works, but agent actions fail and their
# conversation popups stay empty.
harnessFile=examples/ollamaHarness.json
harnessArgs=()

ollamaUrl=$(grep -o '"url": *"[^"]*"' "$harnessFile" | head -n 1 | sed 's/.*"\([^"]*\)"$/\1/')

if curl -s -f -m 5 -o /dev/null "$ollamaUrl/api/tags"; then
	harnessArgs=(-H "$harnessFile")
else
	echo "Ollama server at $ollamaUrl is not reachable; running without an agentic harness."
fi

build/matrytsya_debug -a 127.0.0.1 -k MATRYTSYA_API_TOKEN -n "${harnessArgs[@]}" -d examples/agentHive.mdb
