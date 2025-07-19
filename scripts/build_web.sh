#!/bin/bash

mkdir -p ../build/wasm
emcc ../src/main.qtr -s WASM=1 -o ../build/wasm/index.html
cp -r ../assets ../build/wasm/
zip -r ../build/QuarterCapsule_WebGL.zip ../build/wasm/
