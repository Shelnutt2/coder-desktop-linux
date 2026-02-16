#!/bin/bash
# Build the Go VPN shared library for Linux.
# Usage: ./scripts/build-so.sh [output-path]
set -euo pipefail

cd "$(dirname "$0")/.."
OUTPUT="${1:-libcodervpn.so}"
CGO_ENABLED=1 go build -buildmode=c-shared -o "$OUTPUT" .
echo "Built $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
