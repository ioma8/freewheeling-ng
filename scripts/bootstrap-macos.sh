#!/bin/zsh
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "bootstrap-macos.sh must be run on macOS" >&2
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools are not installed. Run 'xcode-select --install' first." >&2
  exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required. Install it from https://brew.sh first." >&2
  exit 1
fi

typeset -a formulae=(
  sdl2
  sdl2_ttf
  sdl_gfx
  vorbis
  libsndfile
  liblo
  nettle
)

echo "Installing required Homebrew formulae..."
brew install "${formulae[@]}"

echo
echo "macOS bootstrap complete."
echo "Note: Homebrew bottles on this machine may be built for a newer macOS deployment target than the project's 11.0 target."
echo "That can produce linker warnings without breaking local builds."
