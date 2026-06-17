#!/bin/bash

make -C ./build clean
make -C ./build dep
bear -- make -C ./build
