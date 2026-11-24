#!/bin/bash

#build/matrytsya_debug -n examples/gardenHive.json

#build/matrytsya_debug -n examples/seedHive.json

#build/matrytsya_debug -n examples/seedHive.json -d examples/seedHive.mdb

export MATRYTSYA_API_TOKEN=b640a77890aa1f946f5995043e7ef14b60e3c230c33996ffa60fae1174b15def

#build/matrytsya_debug -a 127.0.0.1 -k MATRYTSYA_API_TOKEN -n -d examples/robot.mdb

# The agents in the hive need a model. An OpenRouter API key already in the environment is used as it is;
# otherwise one is asked for. Leave it empty to run without a harness: the scene still works, but agent
# actions fail and their conversation popups stay empty.
harnessArgs=()

if [ -z "$OPENROUTER_API_KEY" ]; then
	read -r -s -p "OpenRouter API key (Enter to run without agents): " OPENROUTER_API_KEY
	echo
fi

if [ -n "$OPENROUTER_API_KEY" ]; then
	export OPENROUTER_API_KEY
	harnessArgs=(-H examples/openRouterHarness.json)
else
	echo "No OpenRouter API key; running without an agentic harness."
fi

build/matrytsya_debug -a 127.0.0.1 -k MATRYTSYA_API_TOKEN -n "${harnessArgs[@]}" -d examples/agentHive.mdb
