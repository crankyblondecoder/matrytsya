#!/bin/bash

# Runs cloc over the project's C++ source, excluding third-party libraries
# (src/mongoose, src/rapidjson, src/lua) and unit tests (src/test).

cloc . \
	--exclude-dir=agents,mongoose,rapidjson,lua,test,build,node_modules,.git,.cache \
	--include-lang='C++,C/C++ Header'
