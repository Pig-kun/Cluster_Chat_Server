# !/bin/bash

set -x

rm -rf ./build/*
cd ./build &&
	cmake .. &&
	make

