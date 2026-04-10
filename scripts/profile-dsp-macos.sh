#!/bin/zsh
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "profile-dsp-macos.sh must be run on macOS" >&2
  exit 1
fi

binary="MacOSX/build/Release/fweelin.app/Contents/MacOS/fweelin"
if [[ ! -x "$binary" ]]; then
  echo "Built app not found. Run 'zsh scripts/build-macos.sh' first." >&2
  exit 1
fi

stdout_log="$(mktemp -t fweelin-dsp-profile-stdout).log"
stderr_log="$(mktemp -t fweelin-dsp-profile-stderr).log"
report_log="$(mktemp -t fweelin-dsp-profile-report).log"

python3 - <<'PY' "$binary" "$stdout_log" "$stderr_log" "$report_log"
import os
import signal
import subprocess
import sys
import time

binary, stdout_log, stderr_log, report_log = sys.argv[1:]
env = os.environ.copy()
env["FW_PROFILE_DSP"] = "1"
env["FW_PROFILE_DSP_OUT"] = report_log

with open(stdout_log, "w") as stdout_fp, open(stderr_log, "w") as stderr_fp:
    proc = subprocess.Popen([binary], stdout=stdout_fp, stderr=stderr_fp, env=env)
    time.sleep(6.0)
    subprocess.run(
        ["osascript", "-e", 'tell application id "com.yourcompany.fweelin" to quit'],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    try:
        proc.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5.0)

print(stdout_log)
print(stderr_log)
print(report_log)
PY

printf 'DSP profiling logs:\n'
printf '  stdout: %s\n' "$stdout_log"
printf '  stderr: %s\n' "$stderr_log"
printf '  report: %s\n' "$report_log"
cat "$report_log"
