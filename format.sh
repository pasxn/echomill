#!/bin/bash
set -e

echo "🔧 Running code formatter (clang-format)..."

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "❌ clang-format not found"
    exit 1
fi

# Parse arguments
DRY_RUN=false
if [[ "$1" == "--check" ]]; then
    DRY_RUN=true
fi

# Find all C++ source files
FILES=$(find src test example extra -type f \( -iname '*.hpp' -o -iname '*.cpp' \) 2>/dev/null)

if [ -z "$FILES" ]; then
    echo "⚠️  No C++ files found"
    exit 0
fi

if [ "$DRY_RUN" = true ]; then
    echo "Checking code format (dry-run)..."
    if ! echo "$FILES" | xargs clang-format --dry-run -Werror; then
        echo "❌ Code formatting check failed"
        echo "💡 To fix, run: ./scripts/format.sh"
        exit 1
    fi
    echo "✅ Code formatting check passed!"
else
    echo "Formatting code..."
    echo "$FILES" | xargs clang-format -i
    echo "✅ Code formatted successfully!"
fi