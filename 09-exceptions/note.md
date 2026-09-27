# 09 — Exceptions: a side table, not an instruction

**Claim:** "zero-cost exceptions" is a claim about the *happy path*, and it is
literally true — the intermediate frames contain no error-handling instructions
at all. The cost was moved into a **table in a separate section**, consulted only
when something actually throws. This chapter measures both sides.

```sh
make table
make raw && grep -A20 GCC_except_table0 out/O1/cleanup.s
```

## 1. The measurement

Four stack frames, one possible failure, two mechanisms:

```
EXCEPTIONS                        ERROR CODES
lvl0_thr    6 insns  0 blocks     lvl0_ec   17 insns  2 blocks  2 spills  x19 x20
lvl1_thr    6 insns  0 blocks     lvl1_ec   17 insns  2 blocks  2 spills  x19 x20
lvl2_thr    6 insns  0 blocks     lvl2_ec   17 insns  2 blocks  2 spills  x19 x20
happy_thr   1 insn                happy_ec  12 insns
```

Per frame: **6 instructions and zero branches, versus 17 instructions and two
basic blocks.** The error-code version must, in every frame: pass an out-pointer,
test the callee's return value, branch, and keep its own state alive across the
call (hence the spills and the callee-saved registers). The exception version
just calls and adds.

`BLOCKS 0` is the headline. The intermediate frames have *no control flow* — they
do not know that an exception is possible. Nothing checks anything.

## 2. Where the cost went

```asm
.cfi_personality 155, ___gxx_personality_v0
.cfi_lsda 16, Lexception1
```

Each function points at a **language-specific data area**, and a personality
routine that knows how to read it. The table itself (clang's own comments):

```asm
GCC_except_table0:
Lcst_begin0:
    .uleb128 Ltmp0-Lfunc_begin0     ; >> Call Site 1 <<
    .uleb128 Ltmp1-Ltmp0            ;   Call between Ltmp0 and Ltmp1
    .uleb128 Ltmp2-Lfunc_begin0     ;     jumps to Ltmp2
    .byte    0                      ;   On action: cleanup
    .uleb128 Ltmp1-Lfunc_begin0     ; >> Call Site 2 <<
    .uleb128 Lfunc_end0-Ltmp1       ;   Call between Ltmp1 and Lfunc_end0
    .byte    0                      ;     has no landing pad
```

That is the entire mechanism: **a map from ranges of code addresses to the
cleanup that applies while the program counter is in that range.** It lives in
`__TEXT,__gcc_except_tab`, not in the instruction stream, so it costs nothing to
run past — but it does cost binary size, and it is why a `.so` full of C++ is
bigger than the same C.

## 3. Throwing is expensive, and that is the deal

`throw` is not a jump. In `chain.s`:

```
___cxa_allocate_exception   heap-allocate the exception object
___cxa_throw                enter the runtime; never returns
___cxa_begin_catch / ___cxa_end_catch    in the frame that catches
__Unwind_Resume             keep unwinding after running local cleanups
```

The unwinder walks return addresses up the stack, and for each frame looks up the
return address in that frame's table, runs the cleanups it names, and either
stops at a matching handler or keeps going. An allocation, a table search, and an
indirect walk — per frame. Hundreds of nanoseconds, not units.

So the trade is explicit: **free when nothing fails, slow when something does.**
Which is correct if failures are rare, and wrong if you use exceptions for
control flow. Only `caught_thr` (10 insns, 1 block, 3 calls) pays anything on the
normal path, and only because it opted in with `try`.

## 4. Destructors are the hard part

`two_guards()` is 43 instructions for two `printf`s, because it must guarantee
`~Guard` runs for `b` then `a` whether `boom()` returns or throws. The landing
pad is a second, invisible exit path through the function, and it is why:

- C code with `goto cleanup` is doing this by hand;
- `-fno-exceptions` makes C++ code genuinely smaller (try it);
- a destructor that throws during unwinding has no defined place to go, which is
  why `std::terminate` is called instead.

## 5. `noexcept` is enforced, not just documented

`promises()` is declared `noexcept` and contains a `Guard` and a call that may
throw. The compiler inserts `__clang_call_terminate` (7 references in
`cleanup.s`): if an exception reaches the boundary, the program dies rather than
propagating. `noexcept` does not mean "cannot throw", it means "unwinding past
here is a bug, so abort". In exchange, callers may omit their own cleanup paths —
which is the optimisation `noexcept` actually buys.

Note `passthrough()` has no destructors and no `catch`, yet still gets 9
instructions and a table entry: the unwinder needs to know this frame has nothing
to do and to keep walking.

## Exercises

1. Rebuild with `EXTRA=-fno-exceptions` (delete `caught_thr` first). How much does
   `two_guards` shrink? Compare total `.s` file sizes too.
2. Add a fifth `lvl4_ec`/`lvl4_thr`. The error-code path grows 17 instructions and
   the exception path grows 6. Extrapolate to a 20-frame call stack.
3. Make `lvl3_thr` `noexcept` while it still throws. What is inserted, and what
   happens at runtime? (Run it.)
4. Compare against `std::expected`/`Result`-style returns: change `lvl*_ec` to
   return a `struct { int value; bool ok; }` by value. Does the branch count
   change? Which one is Rust's model?
5. Find the `Ltmp` labels from the table inside the assembly and mark on paper
   exactly which instruction range is "protected" by the landing pad.
