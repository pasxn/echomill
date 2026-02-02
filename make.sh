#!/bin/bash
# make.sh - Build script for EchoMill components

set -e

DIRECTORIES=("echomill" "client" "e2etest")

function build_dir() {
    local dir=$1
    echo "--- Building $dir ---"
    mkdir -p "$dir/build"
    cd "$dir/build"
    cmake ..
    make -j$(nproc)
    cd ../..
}

function clean_dir() {
    local dir=$1
    echo "--- Cleaning $dir ---"
    rm -rf "$dir/build"
}

case "$1" in
    all)
        for dir in "${DIRECTORIES[@]}"; do
            build_dir "$dir"
        done
        ;;
    clean)
        for dir in "${DIRECTORIES[@]}"; do
            clean_dir "$dir"
        done
        ;;
    echomill|client|e2etest)
        build_dir "$1"
        ;;
    *)
        echo "Usage: $0 {all|echomill|client|e2etest|clean}"
        exit 1
        ;;
esac
