#!/bin/bash
# Hook: enforce 72-character limit on commit subject line (first line)
subject=$(head -1 "$1")
len=${#subject}
if [ "$len" -gt 72 ]; then
  echo "ERROR: Commit subject is $len chars (max 72):" >&2
  echo "  $subject" >&2
  exit 1
fi
