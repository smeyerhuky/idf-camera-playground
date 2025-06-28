#!/bin/bash

# Project-specific build delegation script
# Delegates to the main build system

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_NAME="$(basename "$PROJECT_DIR")"
ROOT_DIR="$(dirname "$(dirname "$PROJECT_DIR")")"

# Delegate to main build system
exec "$ROOT_DIR/build.sh" build "$PROJECT_NAME" "$@"