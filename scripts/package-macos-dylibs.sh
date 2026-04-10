#!/bin/zsh
set -euo pipefail
setopt null_glob

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

app_bundle="MacOSX/build/Release/fweelin.app"
app_binary="$app_bundle/Contents/MacOS/fweelin"
frameworks_dir="$app_bundle/Contents/Frameworks"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "package-macos-dylibs.sh must be run on macOS" >&2
  exit 1
fi

if [[ ! -x "$app_binary" ]]; then
  echo "Built app not found at $app_binary" >&2
  exit 1
fi

mkdir -p "$frameworks_dir"

typeset -A copied
typeset -a queue
queue=("$app_binary")

collect_deps() {
  local target="$1"
  otool -L "$target" | tail -n +2 | awk '{print $1}' | while read -r dep; do
    if [[ "$dep" == /opt/homebrew/* ]]; then
      print -r -- "$dep"
    fi
  done
}

copy_dep() {
  local dep="$1"
  local base="${dep:t}"
  local dest="$frameworks_dir/$base"

  if [[ -n "${copied[$dep]-}" ]]; then
    return 0
  fi

  cp -f "$dep" "$dest"
  chmod u+w "$dest"
  install_name_tool -id "@executable_path/../Frameworks/$base" "$dest"

  copied[$dep]="$dest"
  queue+=("$dest")
}

while (( ${#queue[@]} > 0 )); do
  local_target="${queue[1]}"
  queue=("${queue[@]:1}")
  while read -r dep; do
    [[ -z "$dep" ]] && continue
    copy_dep "$dep"
  done < <(collect_deps "$local_target")
done

rewrite_target() {
  local target="$1"
  while read -r dep; do
    [[ -z "$dep" ]] && continue
    local base="${dep:t}"
    if [[ -e "$frameworks_dir/$base" ]]; then
      install_name_tool -change "$dep" "@executable_path/../Frameworks/$base" "$target"
    fi
  done < <(collect_deps "$target")
}

rewrite_target "$app_binary"
for dylib in "$frameworks_dir"/*.dylib; do
  [[ -e "$dylib" ]] || continue
  rewrite_target "$dylib"
done

for dylib in "$frameworks_dir"/*.dylib; do
  [[ -e "$dylib" ]] || continue
  codesign --force --sign - --timestamp=none "$dylib" >/dev/null
done

codesign --force --sign - --timestamp=none "$app_binary" >/dev/null
codesign --force --deep --sign - --timestamp=none "$app_bundle" >/dev/null

echo "Packaged Homebrew dylibs into $frameworks_dir"
