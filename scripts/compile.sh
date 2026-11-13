#!/usr/bin/env bash
# Usage: scripts/compile.sh input.src -o out.tac
exec "$(dirname "$0")/../bin/compile" "$@"
