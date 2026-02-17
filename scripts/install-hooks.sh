#!/usr/bin/env bash
# Install git hooks for the project
set -euo pipefail
REPO_ROOT="$(git rev-parse --show-toplevel)"
ln -sf "$REPO_ROOT/scripts/pre-commit" "$REPO_ROOT/.git/hooks/pre-commit"
echo "✅ Git hooks installed"
