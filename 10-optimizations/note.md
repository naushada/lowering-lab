# 10 — What the optimiser actually does

**Claim:** lowering gets you *correct* code; passes get you *fast* code. Each pass
is a rewrite you could do by hand and should not, because the compiler will
undo or redo your version anyway. This chapter is the measurement.

```sh
make compare              # -O0 beside -O2
make OPT=O2 remarks       # the compiler explaining itself
```

Instruction counts per function, same source:

```
FUNCTION                 -O0    -O1    -O2      pass
fold                      14      2      2      constant folding
sum_of_squares            26     15     15      inlining -> closed form
div_by_7                   7      7      7      (see below -- O0 uses real sdiv)
div_by_8                   7      5      5      strength reduction
cse                       21      4      4      common subexpression elimination
licm                      30     11     12      loop-invariant code motion
copy_loop                 24      6     44      vectorisation (BIGGER at -O2)
popcount_shift            19      8      8      not recognised: stays a loop
popcount_kernighan        19      5      5      idiom recognition -> cnt
popcount_builtin          11      5      5      the same five instructions
dead                      24      1      1      dead code elimination
threaded                  23      4      4      jump threading
fsum                      25      9     10      NOT vectorised -- see below
```

## 1. Inlining is the enabling pass

`sum_of_squares` at `-O2` contains **no call and no loop**:

```asm
    umull x8, w8, w9          ; n(n-1)
    mul   w9, w8, w9          ; ... (2n-1)
    mov   w10, #21846
    movk  w10, #21845, lsl #16 ; 0x55555556 = magic constant for /3
    madd  w9, w9, w10, w0
```

The compiler inlined `square`, then recognised the loop as a *reduction with a
closed form*, and emitted `n(n-1)(2n-1)/6` — with the division by 3 turned into a
multiply. The loop you wrote does not run at all.

This is why inlining matters more than any other pass: it does not merely remove
a `bl`, it removes the boundary that was stopping constant propagation, CSE, and
loop analysis. Every other pass gets stronger after it.

## 2. Division is a multiply

```asm
_div_by_7:                        ; -O1
    mov   w8, #9363
    movk  w8, #37449, lsl #16     ; 0x92492493
    smull x8, w0, w8              ; multiply by a magic reciprocal
    add   x8, x0, x8, lsr #32
    asr   w9, w8, #2
    add   w0, w9, w8, lsr #31     ; fix up for negative x
```

At `-O0` the same function emits a real `sdiv`. So this is genuinely a *pass*, not
just instruction selection — and the reason is that integer division costs
~20 cycles while multiply costs 3. `div_by_8`, being a power of two, is just
shifts (and still needs a correction for negative numbers, which is why
`x / 8` is not `x >> 3`).

## 3. Idiom recognition matches *shapes*, not meanings

Three functions that compute a population count:

```
popcount_shift      8 insns    while (x) { n += x & 1; x >>= 1; }   loop survives
popcount_kernighan  5 insns    while (x) { x &= x - 1; n++; }       -> cnt.8b
popcount_builtin    5 insns    __builtin_popcount(x)                -> cnt.8b
```

`popcount_kernighan` and `popcount_builtin` compile to the *identical* five
instructions (`fmov`, `cnt.8b`, `addv.8b`, `fmov`). The shift version is equally
correct and stays a loop forever, at every `-O` level.

This is the most practically useful thing in the chapter: pattern matching is
literal. When you want the instruction, either write the recognised shape or call
the builtin — do not hope.

## 4. `-O2` is not "smaller", and `-Os` is a different question

`copy_loop` is the counterexample: 6 instructions at `-O1`, **44 at `-O2`**. The
loop got vectorised into 32-byte chunks (`ldp q0, q1` / `stp q0, q1`) with an
overlap guard and two remainder loops. Faster for large `n`, worse for `n == 3`,
and much bigger. At `-Os` the compiler declines the trade and gives you the
6-instruction loop back. (It never calls `memcpy` here — check with
`grep memcpy`, and note that a compiler is *allowed* to, which is why you cannot
implement `memcpy` itself as a byte loop without `-ffreestanding`.)

`licm` and `fsum` are also very slightly *larger* at `-O2` than `-O1`. "Higher
number, fewer instructions" was never the rule.

## 5. What the compiler is forbidden to do

`fsum` sums floats and is **not** vectorised at any level, while the integer
version in chapter 01 is. Floating-point addition is not associative — `(a+b)+c`
and `a+(b+c)` give different results — so reordering the loop would change the
answer, and the compiler is not permitted to change your answer.

```sh
make OPT=O2 EXTRA=-ffast-math asm   # now it vectorises. You just gave up exactness.
make OPT=O2 remarks                 # ask why, before and after
```

`-ffast-math` is you taking responsibility for the reassociation. Everything the
optimiser refuses to do is a rule like this one: an observable behaviour it is
not allowed to disturb. Which is also why `volatile` works, why a data race is
undefined behaviour (chapter 11), and why `dead` can be deleted but a loop
containing a `printf` cannot.

## Exercises

1. `dead` collapses to one instruction. Add `volatile int t = i * i;` inside the
   loop. Why does the loop come back, and how many instructions does it cost?
2. Compile at `-O2 -fno-inline`. Which of the other improvements in the table
   disappear as collateral damage? That is the measure of inlining's leverage.
3. Make `licm`'s `k` a `volatile int`. Does the hoist survive? Why not?
4. Write `popcount_shift` as an 8-entry table lookup. Compare against the `cnt`
   version, and then time both — is fewer instructions faster here?
5. `make OPT=O2 EXTRA="-Rpass=.*" remarks` and read the full pass log for one
   function. Most of the names are passes that did nothing; find the three that
   mattered.
