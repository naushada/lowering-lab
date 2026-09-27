#!/bin/sh
# Per-function summary of a tidied .asm file.
#   INSNS  instructions        BLOCKS  basic blocks (LBB labels) that survived
#   SPILL  stores to frame     RELOAD  loads from frame
#   CALLS  bl/blr              SAVED   callee-saved registers preserved
# A label starts in column 0. Assembler-local labels (LBB.., Ltmp.., LJTI..) are
# excluded; everything else is a function -- including demangled C++ names, which
# contain colons and parentheses, so we split on the first colon that is followed
# by whitespace, a comment or end of line.
for f in "$@"; do
  echo "== $f"
  printf "%-44s %6s %7s %6s %7s %6s  %s\n" FUNCTION INSNS BLOCKS SPILL RELOAD CALLS SAVED
  awk '
    /^[^ \t]/ {
      if (match($0, /:([ \t;]|$)/)) {
        name = substr($0, 1, RSTART - 1)
        # Assembler-local labels start with L and, unlike a demangled C++ name,
        # contain neither "(" nor "::" -- so LR::f() and main() stay functions
        # while LBB0_1, Lfunc_begin0, Lexception0 and Ltmp3 are dropped.
        if (name ~ /^L/ && name !~ /[(]|::/) {
          if (name ~ /^LBB/) blocks++
          next
        }
        if (name ~ /^(l_|GCC_except|EH_frame|\.L)/) next
        if (fn != "") emit()
        fn = name
        insns = blocks = spill = reload = calls = 0; saved = ""
        next
      }
    }
    fn == "" { next }
    /^[ \t]+[a-z]/ {
      insns++
      if ($0 ~ /^[ \t]+(str|stp|stur|strb|strh)[^;]*\[sp/)  spill++
      if ($0 ~ /^[ \t]+(ldr|ldp|ldur|ldrb|ldrh)[^;]*\[sp/)  reload++
      if ($0 ~ /^[ \t]+(bl|blr)[ \t]/)                      calls++
      n = $0
      while (match(n, /x(19|2[0-8])/)) {
        r = substr(n, RSTART, RLENGTH)
        if (index(saved, r " ") == 0) saved = saved r " "
        n = substr(n, RSTART + RLENGTH)
      }
      next
    }
    END { if (fn != "") emit() }
    function emit() {
      if (insns > 0) printf "%-44s %6d %7d %6d %7d %6d  %s\n", fn, insns, blocks, spill, reload, calls, saved
    }
  ' "$f"
  echo ""
done
