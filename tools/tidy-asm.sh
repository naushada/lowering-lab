#!/bin/sh
# Strip assembler bookkeeping so the instructions are readable.
# Data directives (.long/.quad/.asciz/.byte/.short) are KEPT on purpose:
# that is where jump tables and vtables live.
awk '
  /^[ \t]*\.(cfi_|file|build_version|subsections_via_symbols|no_dead_strip|addrsig|ident|p2align|align|macosx_version|weak_def|private_extern|linker_option|zerofill)/ { next }
  /^[ \t]*\.(section|text|data|const|literal|bss|globl|weak|comm|size|type|hidden|protected|space)/ { next }
  /^[ \t]*;/ { next }
  /^[ \t]*$/ { if (blank++) next; print ""; next }
  { blank = 0; print }
' "$@" | { command -v llvm-cxxfilt >/dev/null && llvm-cxxfilt || cat; }
