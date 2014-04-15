#!/bin/bash

. astylef.sh

eval "$cmd --recursive 'crossbow/*.hpp'"
eval "$cmd --recursive 'crossbow/*.cpp'"
eval "$cmd --recursive 'crossbow/*.h'"
eval "$cmd --recursive 'libs/*.cpp'"
eval "$cmd --recursive 'libs/*.hpp'"
eval "$cmd --recursive 'libs/*.h'"
eval "$cmd --recursive 'test/*.cpp'"
eval "$cmd --recursive 'test/*.h'"
eval "$cmd --recursive 'test/*.hpp'"

