#!/bin/bash

SMALL_FILES_DIR="small_files"
LARGE_FILES_DIR="large_files"

PROGRAMS=("fhistogram" "fhistogram-mt" "fauxgrep" "fauxgrep-mt")

run_benchmark() {
  local program=$1
  local args=$2
  local files=$3
  local threads=$4

  if [ -z "$threads" ]; then
    echo "Running $program on $files"
    /usr/bin/time -p ./$program $args $files/*
  else
    echo "Running $program with $threads threads on $files"
    /usr/bin/time -p ./$program -n $threads $args $files/*
  fi
}

for n in 1 2 4 8; do
  run_benchmark "fhistogram-mt" "" "$SMALL_FILES_DIR" $n
  run_benchmark "fhistogram-mt" "" "$LARGE_FILES_DIR" $n
done

for n in 1 2 4 8; do
  run_benchmark "fauxgrep-mt" "needle" "$SMALL_FILES_DIR" $n
  run_benchmark "fauxgrep-mt" "needle" "$LARGE_FILES_DIR" $n
done
