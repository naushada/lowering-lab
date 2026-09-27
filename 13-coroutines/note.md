# 13 — Coroutines: a function turned inside out

**Claim:** `async`/`await`, generators and `yield` are the one construct in this
whole repo where the *compiler* does the interesting work. There is no hardware
support, no OS involvement, and no thread. The function is **rewritten into a
state machine**, and its locals are moved off the stack into an object.

```sh
make run
make table
```

## 1. Do it by hand first (`manual.c`)

A function that can return in the middle and be resumed needs exactly two things:

1. its locals must not live on the stack — the frame will be gone;
2. something must record *where* to resume.

So: put the locals in a struct, add a `state` field, and `switch` on it at entry.
Suspending is an ordinary `return`. Resuming is an ordinary call. That is the
entire mechanism, and `gen_next` is 25 instructions and 5 basic blocks of it.

Note what this model does **not** need: another stack, a scheduler, or a kernel.
Which is why one thread can hold a million of these.

## 2. Now let the compiler do it (`coro.cpp`)

One `co_yield` and no state machine written by hand produces **three** functions:

```
counter(int)                18 insns   1 call    the "ramp"
counter(int) (.resume)      19 insns   3 blocks  the state machine
counter(int) (.destroy)      2 insns             teardown
```

The ramp:

```asm
counter(int):
    mov  w0, #32
    bl   operator new(unsigned long)       ; a 32-byte FRAME on the heap
    adrp x8, counter(int) (.resume)
    adrp x9, counter(int) (.destroy)
    stp  x8, x9, [x0]                      ; frame[0] = resume, frame[8] = destroy
    stp  w19, w19, [x0, #16]               ; the parameter n, copied in
    str  x0, [x20]                         ; hand the frame back to the caller
```

Calling a coroutine does not run its body. It allocates a frame, writes two
function pointers into the front of it, copies the arguments in, and returns.
Those two pointers are a **two-entry vtable living inside the object** — chapter
06's mechanism again, in a new costume. `coroutine_handle` is just that pointer.

## 3. The state machine, and the frame layout

```asm
counter(int) (.resume):
    ldrb w8, [x0, #28]        ; load the STATE byte
    cbz  w8, LBB4_2           ; state 0 -> first entry
    ldp  w9, w8, [x0, #20]    ; else reload i and n from the frame
    add  w8, w8, #1           ; i++     <- the loop's latch, now in memory
    ...
LBB4_4:
    str  w8, [x0, #24]        ; i      -> frame+24
    mul  w8, w8, w8
    str  w8, [x0, #16]        ; value  -> frame+16  (the promise)
    mov  w8, #1
    strb w8, [x0, #28]        ; state  -> frame+28
    ret                       ; <-- "suspend" is a plain ret
LBB4_5:
    str  xzr, [x0]            ; clear frame[0]: this is how done() answers true
    ret
```

The entire 32-byte frame is readable off those offsets:

```
+0   resume function pointer      (nulled on completion -> done())
+8   destroy function pointer
+16  promise.value                (what co_yield hands back)
+20  n                            (was a parameter)
+24  i                            (was a local)
+28  state                        (was the program counter)
```

Every local became a field; the program counter became a byte. `co_await` and
`co_yield` are the *same* transformation — a numbered suspension point — which is
why generators and `async` functions are one feature in every language that has
them.

## 4. What it costs, and what the optimiser claws back

```
sum_direct     17 insns  0 calls        the plain loop
sum_via_gen    17 insns  0 calls        manual.c's generator -- IDENTICAL
sum_via_coro   25 insns  0 calls        C++20 coroutine
```

Two results worth sitting with:

- The hand-written generator costs **exactly the same** as the direct loop.
  Inlined, the struct became registers again and the state machine folded away.
- `sum_via_coro` makes **no calls at all** — `grep` finds no `operator new` in it.
  The compiler proved the frame does not escape and elided the heap allocation
  entirely (*HALO*: heap allocation elision), then inlined `.resume`. 25 against
  17 instructions for a fully general suspendable function.

But the elision is fragile. It requires the compiler to see the coroutine's whole
lifetime. Return the `Gen` from a function, store it in a container, or cross a
translation unit, and `operator new` comes back — one allocation per coroutine
instance. That is why coroutine-heavy code cares so much about inlining and
custom allocators.

## 5. Where this sits next to chapter 12

| | threads (ch 12) | coroutines (ch 13) |
|---|---|---|
| who implements it | kernel + libc | the compiler |
| suspension is | a context switch (save all registers) | a `ret` (save 1 state byte) |
| stack per task | 512 KB – 8 MB | 32 bytes here |
| preemptible | yes (timer interrupt) | no — it yields, or it never stops |
| parallel | genuinely, on many cores | no: concurrency, not parallelism |

This is the whole argument for async I/O: if a task is waiting rather than
computing, a 32-byte frame and a `ret` beat a kernel thread and a context switch
by several orders of magnitude. And it is also the argument against it: a
coroutine that never suspends blocks everything, because nothing can preempt it.

## Exercises

1. Store the `Gen` in a `std::vector` and rebuild. Confirm `operator new` returns
   and count how many bytes per instance.
2. Add a second `co_yield` in a different branch. How does the state byte's value
   set grow, and does the frame get bigger?
3. Add a local `std::string` that is alive across the `co_yield`, then one that is
   not. Only one of them should land in the frame — check the frame size.
4. Write `manual.c`'s generator with two suspension points and keep it as fast as
   `sum_direct`. Where does that get hard?
5. Read `make ir` for `coro.cpp` at `-O0` and find `llvm.coro.suspend`. The front
   end emits *intrinsics*; a later pass performs the actual split. Which pass?
   (`make OPT=O1 EXTRA="-mllvm -print-pipeline-passes" asm` and look for "coro".)
