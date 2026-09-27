# lowering-lab

Assembly has maybe thirty instructions you care about: move, add, compare, jump,
call, load, store. High-level languages have classes, closures, generics,
exceptions, threads, `async`. **How does the second become the first?**

This repo answers that by measurement rather than assertion. Each chapter is a
small source file plus a `Makefile` that shows you what *your* compiler did with
it — not a textbook's idealised output. Every claim in every note was checked
against the assembly in this repo, and several of my first guesses were wrong.

```sh
git clone https://github.com/naushada/lowering-lab
cd lowering-lab/01-control-flow
make            # readable assembly
make table      # per-function instruction / spill / call counts
make compare    # -O0 beside -O2
make remarks    # the optimiser explaining what it did and refused to do
```

## The idea: a ladder, not a leap

A compiler never jumps from source to assembly. It walks down a ladder, and each
rung deletes exactly one abstraction:

```
source  ──▶  AST  ──▶  high-level IR  ──▶  SSA / 3-address IR  ──▶  machine IR  ──▶  asm
             ▲          ▲                   ▲                       ▲
     types, names,   loops, objects,    unlimited virtual       abstract calls,
     scopes, generics  closures die      registers die          stack layout die
```

By the bottom there is nothing left that needs more than a jump and an add.
**Almost everything in a high-level language exists only at compile time.**

## The five tricks that do all the work

1. **Lowering** — replace a construct with a canned template of simpler ones. A
   `for` loop *is* a compare, a conditional jump, and a jump back.
2. **Address arithmetic** — all data shape collapses to `base + constant` (a
   field) or `base + i*scale` (an index). The CPU has no idea what a struct is.
3. **Indirection** — anything decided at runtime becomes *load an address, then
   call it*. This one trick buys virtual methods, interfaces, closures, jump
   tables, callbacks and coroutine resumption.
4. **Convention** — structure the instruction set cannot express becomes an
   agreed discipline: the ABI. Nothing enforces it; everyone just obeys it.
5. **The runtime library** — whatever cannot be done with instructions at all
   becomes a function *call*: `malloc`, the GC, the exception unwinder, the thread
   scheduler.

## Chapters

| | topic | the finding |
|---|---|---|
| [00](00-instruction-set/note.md) | **the instruction set** | every instruction, what it does, and the C that provokes it — 90 of them from two files |
| [01](01-control-flow/note.md) | control flow | `for` and `while` compile to identical code; your loop counter does not exist; one loop became three at `-O2` |
| [02](02-switch-dispatch/note.md) | switch | one keyword, four lowerings: arithmetic, byte table, binary search, indirect branch — the threshold is measurable |
| [03](03-functions-and-stack/note.md) | functions & stack | four of five recursive functions contain **no call instruction**; returning a big struct never copies |
| [04](04-data-layout/note.md) | data layout | 24 bytes to hold 10; reordering three fields saves a third of the struct |
| [05](05-registers-and-spilling/note.md) | registers & spilling | identical arithmetic, 9 spills vs 4 — the difference is *lifetimes*, not variable count |
| [06](06-objects-and-vtables/note.md) | objects & vtables | a virtual call is three instructions; `final` turns it into `mov w0, #42` |
| [07](07-closures/note.md) | closures | a lambda is a struct with padding; erased vs monomorphised is 13 instructions vs 2 |
| [08](08-generics/note.md) | generics | one template, four copies in the object file — two of them byte-identical |
| [09](09-exceptions/note.md) | exceptions | 6 instructions and **zero branches** per frame, against 17 for error codes |
| [10](10-optimizations/note.md) | optimisation passes | a loop replaced by a closed form; `-O2` makes one function 7x *bigger*; two correct popcounts, one recognised |
| [11](11-atomics-and-memory-order/note.md) | atomics & ordering | `relaxed` is the *same instruction* as no atomic at all — the guarantee is against the compiler |
| [12](12-threads-and-mutex/note.md) | threads | the compiler does nothing; a racy loop was folded into one `add`, losing exactly 1,000,000 updates in 0.0 ms |
| [13](13-coroutines/note.md) | coroutines | locals become struct fields, the program counter becomes one byte, `ret` is the suspend |

Read in order — later chapters lean on earlier ones (spilling explains the cost
of calls; vtables explain coroutine frames) — or jump to whatever you are
debugging today.

## Every chapter has the same targets

| target | what you get |
|---|---|
| `make` | readable assembly in `out/O1/*.asm`, directives stripped, C++ names demangled |
| `make table` | per-function instructions, basic blocks, spills, reloads, calls, callee-saved registers |
| `make compare` | `-O0` beside `-O2`, with instruction counts |
| `make remarks` | the compiler's own report: what it vectorised, unrolled, inlined, and refused to |
| `make ir` | LLVM IR — the rung above assembly, where lowering is still visible |
| `make raw` | untouched assembler output: `.cfi`, sections, exception tables |
| `make dis` | the assembled object, disassembled |
| `make run` | build and execute, where the chapter has something to show |
| `make OPT=O3` / `make EXTRA=-ffast-math` | any flags; output goes to its own directory so nothing is stale |

## Before you start

- [docs/reading-arm64.md](docs/reading-arm64.md) — the one-page version: thirty
  instructions, the register conventions, and an x86-64 translation table. Start
  here if the assembly looks like noise.
- [chapter 00](00-instruction-set/note.md) — the full reference: every instruction
  with a worked example, the flags and condition codes, all the addressing modes,
  and a suffix decoder for guessing an instruction you have never seen.
- These labs were written on **arm64 macOS** with Homebrew clang. They work with
  Apple clang and on Linux too; instruction names in the notes are AArch64, and
  the x86-64 equivalents are in the doc above.
- **Use `-O0` to study the ABI and `-O1`/`-O2` to study the optimiser.** At any
  optimisation level, the compiler's first instinct is to make your example not
  happen — several chapters here are built around exactly that.

## The honest caveat

Compiler output is not a specification. It is what one version of one compiler
chose for one target on one day. Your clang will differ from mine, and next
year's will differ from both. That is *why* the labs generate the assembly instead
of quoting it: when the numbers in a note disagree with what `make table` prints
on your machine, the note is the stale thing, and finding out why the compiler
changed its mind is the most valuable exercise in the repo.
