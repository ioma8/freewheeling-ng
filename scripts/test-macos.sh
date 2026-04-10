#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "test-macos.sh must be run on macOS" >&2
  exit 1
fi

if [[ ! -x MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin ]]; then
  echo "Built app not found. Run 'zsh scripts/build-macos.sh' first." >&2
  exit 1
fi

if otool -L MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin | rg -q '/opt/homebrew'; then
  echo "Packaged app still links against /opt/homebrew dylibs." >&2
  exit 1
fi

zsh scripts/run-runtime-regression-tests.sh
