#!/bin/bash

needle=$1
path=$2

./fauxgrep "$needle" "$path" > tests/output_fauxgrep.txt
./fauxgrep-mt "$needle" "$path" > tests/output_fauxgrep_mt.txt

if diff tests/output_fauxgrep.txt tests/output_fauxgrep_mt.txt > /dev/null; then
    echo "Outputs are identical."
else
    echo "Outputs differ."
fi