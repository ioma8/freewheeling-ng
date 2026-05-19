#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

app_bin="MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin"
if [[ ! -x "$app_bin" ]]; then
  echo "Built app not found at $app_bin" >&2
  exit 1
fi

tmp_home="$(mktemp -d)"
stdout_log="$(mktemp)"
stderr_log="$(mktemp)"
cleanup() {
  rm -f "$stdout_log" "$stderr_log"
  rm -rf "$tmp_home"
}
trap cleanup EXIT

HOME="$tmp_home" "$app_bin" >"$stdout_log" 2>"$stderr_log" &
pid=$!

sleep 6

if kill -0 "$pid" 2>/dev/null; then
  kill "$pid" 2>/dev/null || true
  wait "$pid" || true
  exit 0
fi

set +e
wait "$pid"
exit_code=$?
set -e

echo "Startup regression: app exited too early with code $exit_code" >&2
echo "--- stdout ---" >&2
sed -n '1,160p' "$stdout_log" >&2
echo "--- stderr ---" >&2
sed -n '1,160p' "$stderr_log" >&2
exit 1
