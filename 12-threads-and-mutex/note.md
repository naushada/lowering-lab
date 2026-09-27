# 12 — Threads: the chapter where the compiler steps aside

**Claim:** there is no thread instruction, no parallel construct, and no scheduler
in your binary. `make table` on `primitives.c` and look at what threading
actually compiles to:

```
_spawn              5 insns      -> bl _pthread_create
_reap               1 insn       -> b  _pthread_join      (tail call)
_critical_section  15 insns      -> bl _pthread_mutex_lock / _unlock
_ensure_init        5 insns      -> bl _pthread_once
```

Ordinary function calls. That is the entire lowering. Threads are not a language
feature that the compiler implements; they are a *service* it requests.

```sh
make table
make run          # the race, measured
make OPT=O0 run   # ...and it changes completely. That is the lesson.
```

## 1. Who actually provides threads

| layer | what it contributes |
|---|---|
| **hardware** | several cores, each with its own register set and program counter; the atomics of chapter 11; a timer interrupt for preemption |
| **kernel** | a thread *is* a saved register set plus a stack. `pthread_create` → `bsdthread_create` (Darwin) / `clone` (Linux). A context switch is an interrupt handler storing one register set and loading another |
| **libc** | wraps the syscalls; implements the mutex fast path in user space |
| **compiler** | almost nothing — except *not optimising as though it were alone* |

That last row is the compiler's real job, and section 3 shows what happens when
it does not know to do it.

## 2. A mutex is a CAS that falls back to a syscall

`pthread_mutex_lock` is opaque here, but `critical_section_spin` shows the fast
path it hides: 10 instructions, no calls, a `swpa`/`cbnz` retry loop. A real mutex
does that first, and only when it must actually *block* does it enter the kernel
to sleep (`futex` on Linux, `__ulock_wait` on Darwin) — because blocking means
"take me off the CPU", and only the scheduler can do that.

So an uncontended lock never enters the kernel, which is why `make run` shows a
mutex costing about the same as an atomic, and why the folklore "locks are slow"
is really "*contended* locks are slow, and sleeping is very slow".

## 3. The race, and why `-O0` and `-O1` disagree

```
                       -O1                          -O0
expected            4000000                      4000000
plain   ++          3000000   0.0 ms             1028085   2.6 ms   <- LOST
atomic  relaxed     4000000  40.3 ms             4000000  58.9 ms
atomic  seq_cst     4000000  34.7 ms             4000000  57.0 ms
mutex               4000000  41.4 ms             4000000  42.3 ms
```

Look at `0.0 ms` for the plain counter, then at its assembly:

```asm
_run_plain:                           ; for (i = 0; i < 1000000; i++) plain_counter++;
    ldr  x9, [x8, _plain_counter]
    add  x9, x9, #244, lsl #12        ; + 1000000, in one instruction
    add  x9, x9, #576
    str  x9, [x8, _plain_counter]
    ret
```

**The million-iteration loop is gone.** The compiler folded it into a single
read-modify-write, which is a perfectly legal transformation *for a single
thread* — and a data race is undefined behaviour, so a single thread is all it is
required to consider. Four threads then each performed one RMW, and three of the
four survived: exactly 3,000,000.

At `-O0` the loop is real, the threads interleave for 2.6 ms, and you lose
three-quarters of the updates instead.

Same source, same bug, wildly different symptoms depending on the optimisation
level. This is what "undefined behaviour" means in practice: not "a wrong answer"
but "no stable answer to reason about". Compare `run_relaxed`, whose loop
**survives** with `ldadd` per iteration — the atomic forbade the hoist. Chapter
11's point, visible as a 40 ms difference.

## 4. Thread-local storage needs an indirection

`tls_get` is 9 instructions with a call, not a `ldr` from a fixed address: each
thread must see a different address for the same name, so the address cannot be a
link-time constant. On Darwin the access goes through a thread-local variable
descriptor (an indirect call); on Linux it is usually a register-relative access
off `tpidr_el0`. `thread_local` is cheap but never free.

## 5. What `async`/`await` is not

Nothing in this chapter is what `async` uses. Chapter 13 is the other model: one
thread, many suspended computations, and the *compiler* doing the heavy lifting
for once.

## Exercises

1. Run `make run` five times. Does the plain counter give the same wrong answer
   every time? Now try `make OPT=O2 run`.
2. Make `plain_counter` `volatile` and rerun. The loop comes back and the count is
   still wrong — explain precisely what `volatile` did and did not buy. (This is
   the single most common misconception about `volatile`.)
3. Pin the contention: give each thread its own cache line (`_Alignas(64)` per
   counter) and sum at the end. How much of the 40 ms was false sharing?
4. Replace the mutex with `pthread_spinlock`-style busy waiting at 64 threads
   instead of 4. Which one degrades worse, and why does sleeping win when threads
   outnumber cores?
5. Find the actual syscall: run `sudo dtruss ./out/O1/race 2>&1 | grep -c ulock`
   and compare between the mutex and atomic phases.
