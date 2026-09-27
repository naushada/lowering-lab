# 01 — Control flow

**Claim:** `if`, `while`, `for`, `do`, `break`, `continue`, `&&`, `||` are one
mechanism wearing seven hats: *set flags, then jump on them*. Below the front
end there is no such thing as a loop — only a graph of straight-line blocks.

```sh
make            # readable assembly at -O1
make compare    # what -O2 additionally does to it
make OPT=O0 asm # the dumb, literal translation
```

## 1. `for` and `while` are the same construct

Diff the two functions in `out/loop.asm`. They are identical except for label
numbers. The front end rewrites `for (init; test; step) body` into
`init; while (test) { body; step; }` before anything else happens, so by the
time the optimiser sees them there is nothing left to distinguish.

## 2. The natural shape is bottom-tested, so the compiler rotates every loop

`sum_while` reads as "test, then body". The output reads:

```asm
    cmp   w1, #1
    b.lt  LBB0_4          ; guard: skip the whole loop if n < 1
    ...
LBB0_2:
    ldr   w10, [x0], #4   ; body
    add   w8, w10, w8
    subs  x9, x9, #1      ; latch
    b.ne  LBB0_2          ; test at the BOTTOM
```

The test moved to the bottom and a *guard* was hoisted above the loop. That is
**loop rotation**, and it exists to get one branch per iteration instead of two.
Now compare with `sum_do`, which you wrote bottom-tested by hand: same body, and
it needs no guard because `do` is defined to run once. You wrote the shape the
compiler was going to build anyway.

## 3. Your loop counter does not exist

There is no `i` in that assembly, and no multiply for `a[i]`. Two rewrites:

- **Induction-variable strength reduction** — `&a[i]` is `a + i*4` and `i` only
  ever increases by 1, so the compiler keeps the *address* as the variable and
  bumps it: `ldr w10, [x0], #4` loads and advances `x0` in one instruction.
- **Counting down** — `x9` starts at `n` and counts to zero, because `subs`
  already sets the flags as a side effect of subtracting. Comparing against zero
  is free; comparing `i < n` would cost a `cmp`.

So the loop that survives is not the loop you wrote. This is the single most
common source of confusion when people first read optimised output.

## 4. `break` and `continue` are not features

They are `b` to labels the loop already needed: `continue` jumps to the latch,
`break` jumps to the exit block. `goto` is the same instruction. The reason
`break` feels structured and `goto` feels dangerous is entirely a front-end
story about which labels you are permitted to name.

## 5. `first_negative` compiled to two instructions

```asm
_first_negative:
    mov  w0, #-1
    ret
```

The loop is *gone*. It has no observable effect — it returns `-1` on every path
and touches nothing the caller can see — so **dead code elimination** deleted
it, loads and all. Remember this the next time you time an empty benchmark
loop and it reports 0 ns.

## 6. `&&` is a branch, `&` is arithmetic

In `out/shortcircuit.asm`:

```asm
_and_shortcircuit:      bl _f ; cbz w0, LBB0_2 ; ... bl _f   <- second call may not happen
_and_bitwise:           bl _f ; mov x20, x0 ; ... bl _f      <- both calls, always
```

`&&` produced an extra basic block that can be skipped. `&` produced one block
and an `and` instruction. Two consequences visible right there in the output:

- The evaluation order of `&&` is a guarantee, not an optimisation. `guarded()`
  *must* test `p` before loading `*p`; no compiler may reorder it. That is why
  `p && *p == x` is safe and `(p != 0) & (*p == x)` is not.
- `and_bitwise` had to save `x20` in its prologue — a callee-saved register — to
  hold the first result across the second call. The short-circuit version never
  needs both results alive at once, so it saves one register fewer. Chapter 05.

Also note `cset w0, ne`: the *value* `true` has to be materialised into a
register when it is returned, even though it existed only as a flag. Booleans
are flags inside an `if` and integers everywhere else.

## The rule

A function becomes a **control-flow graph**: basic blocks (straight runs ending
in exactly one branch) joined by edges. Every structured construct in every
language is a template for a small subgraph. Optimisers then work on the graph
and no longer know or care which keyword built it.

## Exercises

1. Make `first_negative` actually `return i`. Watch the loop come back. Now
   return `-1` but print inside the loop — which parts survive?
2. `make compare`. At `-O2` the one loop you wrote has become **three**: a
   vector loop doing 16 ints per iteration (`ldp q4, q5` + four `add.4s`, i.e.
   4 vectors x 4 lanes), a 4-at-a-time loop for the remainder, and a scalar tail
   for what is left. That is why `-O0` is 37 instructions and `-O2` is over 100
   — optimised does not mean shorter.
   Then ask the compiler to justify itself, and to stop:
   ```sh
   make OPT=O2 remarks                          # "vectorized loop (width: 4, interleaved count: 4)"
   make OPT=O2 EXTRA=-fno-vectorize asm         # the scalar shape returns
   ```
3. Rewrite `sign()` as a three-way `if/else if/else`, then as
   `(x > 0) - (x < 0)`. Compare. Which one did the compiler prefer to produce?
4. Add a `goto` version of `sum_while` with explicit labels. Can you make its
   assembly differ from the `while` version at all?
