#!/usr/bin/env bash
# Usage: scripts/run.sh out.tac [-i input.txt]
exec "$(dirname "$0")/../bin/run" "$@"
