#!/bin/bash

#build/matrytsya_debug -n examples/gardenHive.json

#build/matrytsya_debug -n examples/seedHive.json

#build/matrytsya_debug -n examples/seedHive.json -d examples/seedHive.mdb

export MATRYTSYA_API_TOKEN=b640a77890aa1f946f5995043e7ef14b60e3c230c33996ffa60fae1174b15def

#build/matrytsya_debug -a 127.0.0.1 -k MATRYTSYA_API_TOKEN -n -d examples/robot.mdb

build/matrytsya_debug -a 127.0.0.1 -k MATRYTSYA_API_TOKEN -n -d examples/agentHive.mdb
