#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

icon_src="extracted-assets/freewheeling-icon-128_upscayl_4x_ultrasharp-4x.png"
icon_dst="MacOSX/freewheeling.icns"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "generate-macos-icon.sh must be run on macOS" >&2
  exit 1
fi

if [[ ! -f "$icon_src" ]]; then
  echo "Icon source not found at $icon_src" >&2
  exit 1
fi

iconset_dir="$(mktemp -d /tmp/freewheeling.iconset.XXXXXX).iconset"
cleanup() {
  rm -rf "$iconset_dir"
}
trap cleanup EXIT

mkdir -p "$iconset_dir"

cp "$icon_src" "$iconset_dir/icon_512x512.png"
sips -z 16 16 "$icon_src" --out "$iconset_dir/icon_16x16.png" >/dev/null
sips -z 32 32 "$icon_src" --out "$iconset_dir/icon_16x16@2x.png" >/dev/null
sips -z 32 32 "$icon_src" --out "$iconset_dir/icon_32x32.png" >/dev/null
sips -z 64 64 "$icon_src" --out "$iconset_dir/icon_32x32@2x.png" >/dev/null
sips -z 128 128 "$icon_src" --out "$iconset_dir/icon_128x128.png" >/dev/null
sips -z 256 256 "$icon_src" --out "$iconset_dir/icon_128x128@2x.png" >/dev/null
sips -z 256 256 "$icon_src" --out "$iconset_dir/icon_256x256.png" >/dev/null
sips -z 512 512 "$icon_src" --out "$iconset_dir/icon_256x256@2x.png" >/dev/null

iconutil -c icns "$iconset_dir" -o "$icon_dst"
echo "Generated $icon_dst from $icon_src"
