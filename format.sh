#!/bin/bash

if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed. Please install and try again."
    exit 1
fi

find echomill -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +
