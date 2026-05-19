#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if ! plutil -extract NSHighResolutionCapable raw MacOSX/Info.plist >/dev/null 2>&1; then
  echo "MacOSX/Info.plist is missing NSHighResolutionCapable." >&2
  exit 1
fi

if [[ "$(plutil -extract NSHighResolutionCapable raw MacOSX/Info.plist)" != "true" ]]; then
  echo "NSHighResolutionCapable is not enabled in MacOSX/Info.plist." >&2
  exit 1
fi

if ! rg -q 'SDL_WINDOW_ALLOW_HIGHDPI' src/fweelin_videoio.cc; then
  echo "src/fweelin_videoio.cc does not request SDL_WINDOW_ALLOW_HIGHDPI." >&2
  exit 1
fi
