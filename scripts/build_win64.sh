#!/bin/bash

mkdir -p ../build/win64
quarterc ../src/main.qtr -o ../build/win64/QuarterCapsule.exe --arch x86_64 --link-sdl2 --link-gtk3
cp -r ../assets ../build/win64/
zip -r ../build/QuarterCapsule_Win64.zip ../build/win64/
