#!/bin/bash
# Hook: enforce 65-character limit on commit subject line (first line).
# GitHub squash-merge appends " (#NNN)" (up to 7 chars) when closing a PR,
# so the effective post-merge limit is 65+7=72 — the conventional git maximum.
subject=$(head -1 "$1")
len=${#subject}
if [ "$len" -gt 65 ]; then
  echo "ERROR: Commit subject is $len chars (max 65, leaves room for GitHub PR ref):" >&2
  echo "  $subject" >&2
  exit 1
fi
