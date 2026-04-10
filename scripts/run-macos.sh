#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "run-macos.sh must be run on macOS" >&2
  exit 1
fi

app_binary="MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin"

if [[ ! -x "$app_binary" ]]; then
  echo "Built app not found. Running build first..."
  zsh scripts/build-macos.sh
fi

exec "$app_binary" "$@"
