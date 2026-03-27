#!/bin/bash
# Hook: #pragma once must not appear as a preprocessor directive in .cpp files.
# Matches only lines where # is the first non-whitespace character, ignoring
# occurrences inside comments or string literals.
failed=0
for f in "$@"; do
  if grep -Pq '^\s*#\s*pragma\s+once' "$f"; then
    echo "ERROR: #pragma once directive in $f (only allowed in .h files)" >&2
    failed=1
  fi
done
exit $failed
