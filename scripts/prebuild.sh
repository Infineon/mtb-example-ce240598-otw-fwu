#!/bin/bash

#delete existing hex files, to avoid double header/signature during postbuild

#input params
COMPILED_HEX=$1
#COMPILED_HEX_UNSIGNED=$2

if [ -e "$COMPILED_HEX.hex" ]; then
    rm "$COMPILED_HEX.hex"
    echo "$COMPILED_HEX.hex deleted"
fi

#if [ -e "$COMPILED_HEX_UNSIGNED.hex" ]; then
#    rm "$COMPILED_HEX_UNSIGNED.hex"
#    echo "$COMPILED_HEX_UNSIGNED.hex deleted"
#fi