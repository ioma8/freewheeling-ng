# ralph.sh
# Usage: ./ralph.sh <iterations>

set -e

if [ -z "$1" ]; then
  echo "Usage: $0 <iterations>"
  exit 1
fi

# For each iteration, run codex with the following prompt.
# This prompt is basic, we'll expand it later.
for ((i=1; i<=$1; i++)); do
  result=$(codex exec --yolo \
" \
Check file progress.txt for the current progress of this repo. If not existent, create empty. \
Check this repo for any weaknesses, performance issues, or security vulnerabilities. \
Resolve the most significant one fully and when finished update progress.txt with the following format: 'Iteration X: [description of the issue resolved]'. \
Work only on one issue at a time.
If there are no issues, write 'Iteration X: No issues found.' \
")

  echo "$result"
done
