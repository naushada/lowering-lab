# 03 — Functions, the stack, and recursion

**Claim:** there is no "function" in hardware. There is `bl` (jump, save the
return address in `lr`), `ret` (jump to `lr`), one stack-pointer register, and a
**treaty** about who may clobber what. Everything else — parameters, locals,
scope, recursion — is that treaty being obeyed.

```sh
make table
make OPT=O0 asm    # IMPORTANT: -O0 shows the treaty literally. -O1 hides it.
```

## 1. The treaty (AArch64 / Apple)

| | |
|---|---|
| arguments | `x0`–`x7`, left to right; the 9th onward go **on the stack, pushed by the caller** |
| return | `x0` (and `x1` for a 16-byte pair) |
| big return | caller allocates the space and passes its address in `x8` |
| free to clobber | `x0`–`x18` |
| must preserve | `x19`–`x28`, `x29` (fp), `sp` |
| alignment | `sp` 16-byte aligned at every call |

Nothing enforces this. Violate it and there is no trap — just wrong answers.
That is the sense in which the ABI is a *convention*: the compiler and the
hardware are not the same authority.

## 2. `many()` and the ninth argument

`many` shows `RELOAD 1` in the table: one `ldr w8, [sp]` — the 9th argument,
which the caller left on the stack. The first eight cost nothing to pass; the
ninth costs a store and a load. That is the real reason style guides limit
parameter counts, and the reason a struct-of-args is often *cheaper* than many
args.

`call_many` at `-O1` is a punchline:

```asm
_call_many:
    mov  w0, #45        ; 1+2+...+9, computed at compile time
    ret
```

The entire call disappeared. **Use `make OPT=O0 asm` when you want to study the
ABI**, because at any optimisation level the compiler's first instinct is to
make your example not happen.

## 3. Leaf functions need no frame

`leaf` is 3 instructions with no prologue: nothing must be preserved because it
calls nothing, so nothing can clobber it. Then compare `not_leaf`, which does
the same arithmetic but calls `other()`: `SPILL 2`, `SAVED x19 x20`, and a
prologue appears. The cost of the call is not just the `bl` — it is that your
live values suddenly need protected homes (chapter 05).

## 4. Returning a big struct is a lie

```asm
_make_big:                    ; struct big make_big(long v)
    stp  x0, x9,  [x8]        ; writes straight through x8
    stp  x9, x10, [x8, #16]
    str  x9, [x8, #32]
```

40 bytes do not fit in registers, so the *caller* allocated the space and passed
its address in `x8`. "Return by value" never copies at the return; the callee
constructs the value in the caller's storage. This is also exactly the mechanism
C++ copy elision / RVO is defined in terms of — the optimisation was always the
natural implementation, and the copy was the fiction.

## 5. Recursion is free; *frames* are what cost

`make table` on `recursion.c`, and look at the CALLS column:

```
_fact_rec     9 insns   0 calls     <- ?!
_fact_tail    8 insns   0 calls
_fact_iter   11 insns   0 calls
_is_odd       6 insns   0 calls
_is_even      5 insns   0 calls
_fib         16 insns   1 call      <- only one of its two calls survived
```

Four of these recursive functions contain **no call instruction at all**.

- `fact_tail` is a tail call: nothing happens after the callee returns, so the
  frame can be reused and `bl; ret` becomes a jump — which is then just a loop.
  Read it: `mul x1, x1, x0; sub x0, x0, #1; b.ge`. Your recursion is now `while`.
- `fact_rec` is *not* a tail call — `n * fact(n-1)` multiplies after returning —
  and it still became a loop. LLVM recognised that `*` is associative and
  introduced an accumulator (*accumulator recursion elimination*). The
  transformation you were told to do by hand was done for you.
- `is_odd`/`is_even` are *mutually* recursive, both in tail position, and both
  collapsed.
- `fib` cannot fully collapse — `fib(n-1) + fib(n-2)` needs two results — but one
  of the two calls is in tail position and was eliminated. One `bl` remains, and
  `SPILL 2 / SAVED x19 x20` is the surviving frame.

So "recursion is slow" is wrong as stated. Recursion costs when the frame must
**stay alive** across the call, which is a property of what you do with the
result, not of recursion. Verify by reading the same file at `-O0`, where every
call is a real call and each frame is real.

## 6. Frames and scope

In `frame.asm`:

- `big_frame` moves `sp` down once by a constant. There is no per-variable push;
  the whole frame is one subtraction.
- `vla(int n)` cannot do that — the size is unknown at compile time — so `sp` is
  adjusted by a computed amount and must be restored from `x29` afterwards. That
  is what the frame pointer is *for*.
- `reuse()` has two arrays in two disjoint scopes and gives them the **same stack
  slot**. Scope is a compile-time story about which names are visible; the
  storage is recycled by liveness. This is also why reading an out-of-scope
  pointer sometimes "works" and sometimes returns the other variable.

## Exercises

1. `make OPT=O0 asm` and find the `str` in `call_many` that plants the 9th
   argument on the stack. Now count total instructions at `-O0` vs `-O1`.
2. Make `fact_rec` use a non-associative operation (`n - fact_rec(n-1)`). Does
   the accumulator trick still apply? Should it?
3. Add `__attribute__((noinline))` to `leaf` and call it from `not_leaf`. Who
   grows a prologue?
4. Recurse `fib` at `-O0` and measure the frame size from the prologue. Divide
   8 MB by it: that is your real recursion depth limit. Then test the prediction.
