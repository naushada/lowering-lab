#!/bin/sh
# Distinct instruction mnemonics in a tidied .asm, with counts.
# Useful for "what did this file actually use?" and for checking that a lab
# really provokes the instruction it claims to.
for f in "$@"; do
  echo "== $f"
  grep -hoE '^[[:space:]]+[a-z][a-z0-9_.]*' "$f" \
    | tr -d ' \t' \
    | sed 's/^\.//' \
    | grep -vE '^(loh|byte|long|quad|short|asciz|ascii|space|zero|word)$' \
    | sort | uniq -c | sort -rn \
    | awk '{printf "%6d  %s\n", $1, $2}'
  echo ""
done
