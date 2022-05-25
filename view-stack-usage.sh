#!/bin/sh

find cmake-build-debug | grep -e \\.su | xargs cat | awk -v FS='\t' '{ gsub(ENVIRON["PWD"],"",$1); print$1,$2,$3}'| sort -n -k 2 | column -t
