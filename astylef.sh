#!/bin/bash

cmd="astyle --suffix=none --style=java --indent=spaces=4 --indent-col1-comments --pad-oper --pad-header --unpad-paren --align-pointer=type --align-reference=name"

eval "$cmd $@"
