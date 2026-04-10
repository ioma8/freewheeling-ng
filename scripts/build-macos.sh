#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build-macos.sh must be run on macOS" >&2
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools are not installed. Run 'xcode-select --install' first." >&2
  exit 1
fi

xcodebuild -project MacOSX/fweelin.xcodeproj -configuration Release clean build
zsh scripts/package-macos-dylibs.sh
