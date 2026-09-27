# 05 — Registers, spilling, and why lifetimes are the real cost

**Claim:** the IR the optimiser works on has unlimited variables; the machine has
about thirty registers, of which a function can freely use maybe fifteen. Closing
that gap is **register allocation**, and it is graph colouring: two values that
are alive at the same moment interfere and need different registers. When the
colours run out, a value is *spilled* to the stack and reloaded.

```sh
make table
```

```
FUNCTION          INSNS  SPILL  RELOAD  CALLS  SAVED
_low                  7      0       0      0
_forced_spills       59      9      10      1   x19..x28   <- all ten, plus stack
_short_lived         47      4       4      1   x19..x24
_across_calls        25      3       3      4   x19..x22
_address_taken        9      2       2      1   x19 x20
```

## 1. What a spill looks like

`forced_spills` loads sixteen values, calls `barrier()`, then needs all sixteen.
AArch64 has exactly ten callee-saved integer registers, so ten values get one
each — that is the `SAVED x19..x28` — and the remaining six go to stack slots.
Every `str ..., [sp, #N]` before the call and matching `ldr` after it is the
allocator admitting defeat.

## 2. The same arithmetic, four times cheaper

`short_lived` performs **the identical sixteen loads and sixteen multiplies**,
with the same call in the middle. It spills 4 instead of 9 and needs six
callee-saved registers instead of ten. The only difference is that each value is
folded into the accumulator immediately, so no two of them are ever alive
together.

This is the practical lesson of the chapter, and it is not "use fewer
variables" — it is **shorten lifetimes**. A hundred variables used one at a time
are free; six variables all alive across a call are not.

## 3. A call is a wall

`across_calls` has only four values but four calls, and needs four callee-saved
registers — one per value that has to survive a wall. Callee-saved registers are
not free either: the prologue and epilogue pay for them (`stp`/`ldp`), whether or
not the call is ever reached.

This is the honest cost of a function call, and why inlining is the enabling
optimisation for everything else: it does not just remove the `bl`, it removes
the wall, and then the allocator no longer has to protect anything.

## 4. `&x` defeats the register file by definition

`address_taken` spills a value that is used twice and is obviously hot, because
`&x` demands that `x` have a real location. The compiler may keep a copy in a
register, but it must be prepared for memory to be the truth.

Taking an address is therefore a performance decision, not just a syntactic one.
It is also why `restrict` exists, why escape analysis matters in managed
languages, and why passing small structs by value can beat passing them by
pointer.

## 5. Why `-O0` code is so slow

At `-O0` there is no allocation at all: every variable gets a stack slot, and
every operation is load-operate-store. Try it:

```sh
make OPT=O0 table    # compare INSNS and SPILL against -O1
```

The gap between `-O0` and `-O1` is mostly this one pass. Nothing clever — just
keeping values in registers.

## Exercises

1. Cut `forced_spills` from 16 values to 10, then 8. At what count do the spills
   reach zero? Does that match the ten callee-saved registers, and why not
   exactly?
2. Move the `barrier()` call to the *end* of `forced_spills`. All spilling should
   vanish. Explain in terms of the interference graph.
3. Mark `barrier` as `__attribute__((const))` and rebuild. What did you just
   promise the compiler, and which spills did that promise buy back?
4. Add `float` versions of the sixteen values. Do they compete for the same
   registers? (Count `v0`–`v31`.) What does that tell you about mixing integer
   and floating-point work in a hot loop?
