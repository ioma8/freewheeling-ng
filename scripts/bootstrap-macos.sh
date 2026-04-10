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

brewfile="third_party/macos/Brewfile"

if [[ ! -f "$brewfile" ]]; then
  echo "Missing Brewfile at $brewfile" >&2
  exit 1
fi

echo "Installing required Homebrew formulae from $brewfile..."
brew bundle --file "$brewfile"

echo
echo "macOS bootstrap complete."
echo "The build now packages Homebrew dylibs into the app bundle so runtime does not depend on /opt/homebrew."
