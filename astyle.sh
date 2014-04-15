#!/bin/bash

cmd="astyle --suffix=none --style=java --indent=spaces=4 --indent-col1-comments --pad-oper --pad-header --unpad-paren --align-pointer=type --align-reference=name --recursive"

eval "$cmd 'crossbow/*.hpp'"
eval "$cmd 'crossbow/*.cpp'"
eval "$cmd 'crossbow/*.h'"
eval "$cmd 'libs/*.cpp'"
eval "$cmd 'libs/*.hpp'"
eval "$cmd 'libs/*.h'"
eval "$cmd 'test/*.cpp'"
eval "$cmd 'test/*.h'"
eval "$cmd 'test/*.hpp'"

