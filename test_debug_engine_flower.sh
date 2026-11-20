#!/bin/bash

# Mirrors src/matrytsya_test.cpp: the engine and flower hives on one server at 127.0.0.1:8080, run against the
# Ollama harness with chat at LOW capability, two threads per hive and model traffic logged to the console.
# Port, address, chat capability, thread count and shading are left at their defaults, which match the test.

build/matrytsya_debug -v -H examples/ollamaHarness.json examples/engineHive.json examples/flowerHive.json

#build/matrytsya_debug -v -H examples/ollamaHarness.json examples/engineHive.json examples/flowerHive.json examples/roseHive.json
