# 11 — Atomics: the only concurrency the hardware offers

**Claim:** the machine gives you exactly two concurrency primitives — atomic
read-modify-write, and ordering constraints — and *everything* (mutexes,
channels, queues, garbage collectors, `async` runtimes) is built from those two
plus the OS (chapter 12).

```sh
make
make EXTRA="-mcpu=apple-m1+nolse" asm      # the same file, pre-2014 hardware
```

## 1. The whole vocabulary, one table

Every function in `atomics.c`, reduced to the instruction it became:

| C | arm64 | what it is |
|---|---|---|
| `plain++` | `ldr` / `add` / `str` | **three** instructions — a data race |
| `fetch_add(relaxed)` | `ldadd` | atomic add, no ordering |
| `fetch_add(seq_cst)` | `ldaddal` | atomic add, acquire **a**nd re**l**ease |
| `load(relaxed)` | `ldr` | *the same instruction as a normal load* |
| `load(acquire)` | `ldapr` | nothing after it may be hoisted before it |
| `load(seq_cst)` | `ldar` | acquire, plus total-order participation |
| `store(relaxed)` | `str` | *the same instruction as a normal store* |
| `store(release)` | `stlr` | nothing before it may sink after it |
| `store(seq_cst)` | `stlr` | same instruction as release, on this ISA |
| `thread_fence(acq_rel)` | `dmb ish` | a standalone barrier, no data |
| `compare_exchange_strong` | `casal` | CAS |
| `exchange(acquire)` | `swpa` | atomic swap |
| `atomic_is_lock_free` | `mov w0, #1` | answered at **compile** time |

## 2. `relaxed` is the same instruction as no atomic at all

This is the single most clarifying line in the table: `atomic_load_explicit(...,
relaxed)` emits `ldr`, exactly like a plain read. So what did `atomic` buy?

Nothing from the CPU — everything from the **compiler**. A relaxed atomic forbids
the compiler from tearing the access, duplicating it, inventing it, deleting it,
or fusing it with its neighbours. A plain `int` read in a racing program is
undefined behaviour, so the optimiser is allowed to assume it never happens, and
things like "hoist the load out of the loop" then break your spin-wait.

So `atomic` is mostly a contract with the *compiler*, and only sometimes an
instruction. That is why `volatile` is not a substitute (it constrains the
compiler but implies no ordering and no atomicity) and why a data race is UB
rather than merely "a wrong answer".

## 3. Release/acquire is one instruction, not a fence

`stlr` and `ldar`/`ldapr` are *ordered load and store instructions*. You only pay
for a separate `dmb ish` barrier when you ask for a standalone fence. This is why
acquire/release is the ordering you should reach for: on this ISA it is free
relative to a plain load/store in instruction count, and it costs only the
reordering the CPU can no longer do.

Note also that `store(release)` and `store(seq_cst)` are **the same instruction
here**. The difference in cost between release and seq_cst is architecture
dependent — on x86-64 both loads are a plain `mov` and a seq_cst *store* needs
`xchg` or `mfence`. Portable code that "optimises" by weakening seq_cst to
release may buy literally nothing on one machine and a great deal on another.
Measure on the target, not on the model.

## 4. CAS, and what "lock-free" really costs

```asm
_lock:                       ; while (exchange(&flag, 1, acquire)) ;
    swpa w9, w10, [x8]
    cbnz w10, LBB13_1
_unlock:
    stlr wzr, [x8]
```

A complete spinlock: two instructions to take, one to release. `atomic_max` shows
the retry loop that every lock-free algorithm bottoms out in — `casl`, compare,
branch back — and the loop is the price: under contention you do the work
repeatedly and throw it away. Lock-free means *nobody blocks*, not *nobody waits*.

## 5. What this looked like before 2014

`make EXTRA="-mcpu=apple-m1+nolse" asm` removes the ARMv8.1 Large System
Extensions, and one atomic increment becomes a loop:

```asm
LBB1_1:
    ldxr  w9, [x8]         ; load-exclusive: start watching this address
    add   w9, w9, #1
    stxr  w10, w9, [x8]    ; store-exclusive: fails if anyone touched it
    cbnz  w10, LBB1_1      ; ...so try again
```

This is the classic LL/SC (load-linked / store-conditional) pattern, and it is
what RISC-V and pre-8.1 ARM still use. The single-instruction `ldadd` is the same
algorithm implemented in hardware. Worth knowing because the LL/SC form explains
the rules: you cannot do much between the `ldxr` and the `stxr`, and the retry
loop is why "atomic" and "wait-free" are not synonyms.

## Exercises

1. Run `inc_plain` from two threads a million times each (chapter 12 has the
   harness) and print the total. How far below 2,000,000 do you land, and does
   `-O2` make the loss bigger or smaller?
2. Change the spinlock's acquire to `relaxed`. The instruction changes from `swpa`
   to `swp`. Write down which reordering that permits, and what breaks.
3. `atomic_llong` reports lock-free here. Try `struct { long a, b; }` with
   `_Atomic` — is it still? Look for a call to `__atomic_load` (a library lock).
4. Compile `atomics.c` for x86-64 if you have a cross toolchain
   (`--target=x86_64-linux-gnu`) and rebuild the table. Which rows change?
5. Write a Treiber stack push with `compare_exchange_weak`. Then explain, from the
   assembly, why the ABA problem is not visible anywhere in it.
