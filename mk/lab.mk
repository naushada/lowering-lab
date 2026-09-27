# Shared rules for every chapter. A chapter Makefile is just:
#
#   SRCS = foo.c bar.cpp
#   RUN  = bar.cpp          # optional: sources with a main() worth running
#   include ../mk/lab.mk
#
# Output goes to out/<OPT>/ so that changing the -O level really rebuilds:
#   make            asm at the chapter's default -O   -> out/O1/*.asm
#   make OPT=O2     the same sources at -O2           -> out/O2/*.asm
#   make ir | dis | compare | run | remarks | clean | help

ROOT   := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
# make predefines CC=cc, so ?= never fires; only replace the built-in default.
ifeq ($(origin CC),default)
  CC := clang
endif
ifeq ($(origin CXX),default)
  CXX := clang++
endif
OPT    ?= O1
CSTD   ?= -std=c17
CXXSTD ?= -std=c++20
WARN   ?= -Wall
EXTRA  ?=
LDLIBS ?=

COMMON  = $(WARN) $(EXTRA)
# Output path encodes the flags, so changing -O or EXTRA really rebuilds instead
# of silently reusing a stale file. A chapter's own EXTRA stays untagged; flags
# you pass on the command line get their own directory.
ifeq ($(origin EXTRA),command line)
  TAG := -$(shell printf '%s' '$(EXTRA)' | tr -cd '[:alnum:]' | cut -c1-20)
else
  TAG :=
endif
OUT    := out/$(OPT)$(TAG)
TIDY   := sh $(ROOT)/tools/tidy-asm.sh
COUNT  := sh $(ROOT)/tools/count-insns.sh
NAMES  := $(basename $(SRCS))
REMARK := -Rpass='(loop-vectorize|loop-unroll|inline)' -Rpass-missed='(loop-vectorize|loop-unroll)'

.PHONY: asm raw ir dis compare run remarks table mnemonics clean help
.DEFAULT_GOAL := asm

asm: $(NAMES:%=$(OUT)/%.asm)
ir:  $(NAMES:%=$(OUT)/%.ll)
dis: $(NAMES:%=$(OUT)/%.dis)

$(OUT):
	@mkdir -p $(OUT)

# --- readable assembly ----------------------------------------------------
$(OUT)/%.asm: %.c | $(OUT)
	$(CC) $(CSTD) -$(OPT) $(COMMON) -S -o $(OUT)/$*.raw $<
	@$(TIDY) $(OUT)/$*.raw > $@ && rm -f $(OUT)/$*.raw
	@echo "  $@  [`$(COUNT) $@` instructions]"

$(OUT)/%.asm: %.cpp | $(OUT)
	$(CXX) $(CXXSTD) -$(OPT) $(COMMON) -S -o $(OUT)/$*.raw $<
	@$(TIDY) $(OUT)/$*.raw > $@ && rm -f $(OUT)/$*.raw
	@echo "  $@  [`$(COUNT) $@` instructions]"

raw: $(NAMES:%=$(OUT)/%.s)

# --- untidied assembler output: directives, .cfi, exception tables ------
$(OUT)/%.s: %.c | $(OUT)
	$(CC) $(CSTD) -$(OPT) $(COMMON) -S -o $@ $<
	@echo "  $@"

$(OUT)/%.s: %.cpp | $(OUT)
	$(CXX) $(CXXSTD) -$(OPT) $(COMMON) -S -o $@ $<
	@echo "  $@"

# --- LLVM IR: the rung above assembly, still portable, already lowered ----
$(OUT)/%.ll: %.c | $(OUT)
	$(CC) $(CSTD) -$(OPT) $(COMMON) -S -emit-llvm -o $@ $<
	@echo "  $@"

$(OUT)/%.ll: %.cpp | $(OUT)
	$(CXX) $(CXXSTD) -$(OPT) $(COMMON) -S -emit-llvm -o $@ $<
	@echo "  $@"

# --- real bytes: assemble, then disassemble ------------------------------
$(OUT)/%.dis: %.c | $(OUT)
	$(CC) $(CSTD) -$(OPT) $(COMMON) -c -o $(OUT)/$*.o $<
	@objdump -d --no-show-raw-insn $(OUT)/$*.o > $@
	@echo "  $@"

$(OUT)/%.dis: %.cpp | $(OUT)
	$(CXX) $(CXXSTD) -$(OPT) $(COMMON) -c -o $(OUT)/$*.o $<
	@objdump -d --no-show-raw-insn $(OUT)/$*.o | llvm-cxxfilt > $@
	@echo "  $@"

mnemonics: asm
	@sh $(ROOT)/tools/mnemonics.sh $(NAMES:%=$(OUT)/%.asm)

# --- per-function cost table ---------------------------------------------
table: asm
	@sh $(ROOT)/tools/insn-table.sh $(NAMES:%=$(OUT)/%.asm)

# --- -O0 against -O2 ------------------------------------------------------
compare:
	@$(MAKE) --no-print-directory OPT=O0 asm >/dev/null
	@$(MAKE) --no-print-directory OPT=O2 asm >/dev/null
	@for n in $(NAMES); do \
	  a=out/O0/$$n.asm; b=out/O2/$$n.asm; \
	  echo ""; echo "=== $$n:  -O0 `$(COUNT) $$a` insns   ->   -O2 `$(COUNT) $$b` insns"; \
	  diff -y --width=156 --suppress-common-lines $$a $$b | head -44 || true; \
	  echo "... full text in $$a and $$b"; \
	done

# --- the compiler explaining itself --------------------------------------
remarks:
	@for s in $(SRCS); do \
	  echo "=== $$s at -$(OPT)"; \
	  case $$s in \
	    *.cpp) $(CXX) $(CXXSTD) -$(OPT) $(COMMON) $(REMARK) -c -o /dev/null $$s 2>&1 | grep -E 'remark|^ *[0-9]+ \|' || echo "  (no remarks)" ;; \
	    *)     $(CC)  $(CSTD)   -$(OPT) $(COMMON) $(REMARK) -c -o /dev/null $$s 2>&1 | grep -E 'remark|^ *[0-9]+ \|' || echo "  (no remarks)" ;; \
	  esac; \
	done

run: | $(OUT)
	@for s in $(RUN); do \
	  n=`basename $$s .c`; n=`basename $$n .cpp`; \
	  case $$s in \
	    *.cpp) $(CXX) $(CXXSTD) -$(OPT) $(COMMON) -o $(OUT)/$$n $$s $(LDLIBS) ;; \
	    *)     $(CC)  $(CSTD)   -$(OPT) $(COMMON) -o $(OUT)/$$n $$s $(LDLIBS) ;; \
	  esac; \
	  echo "--- ./$(OUT)/$$n"; ./$(OUT)/$$n; echo "[exit $$?]"; \
	done

clean:
	@rm -rf out

help:
	@echo "chapter sources: $(SRCS)"
	@echo "  make            assembly at -$(OPT)         -> $(OUT)/*.asm"
	@echo "  make OPT=O2     same, at -O2                -> out/O2/*.asm"
	@echo "  make EXTRA=-flag ad-hoc flags, own output dir -> out/$(OPT)-<flag>/"
	@echo "  make raw        assembler output untouched (directives, unwind tables)"
	@echo "  make ir         LLVM IR (one rung up)       -> $(OUT)/*.ll"
	@echo "  make dis        disassembled object         -> $(OUT)/*.dis"
	@echo "  make compare    -O0 against -O2, side by side"
	@echo "  make mnemonics  every distinct instruction used, with counts"
	@echo "  make table      per-function instruction/spill/call counts"
	@echo "  make remarks    what the optimiser says it did, and what it refused"
	@echo "  make run        build and execute: $(RUN)"
