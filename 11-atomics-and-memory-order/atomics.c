/* Concurrency is where the compiler stops being able to hide the machine.
   There is no "thread" instruction (chapter 12), but there ARE three hardware
   primitives, and every lock, queue and channel in existence is built from them:

     1. atomic read-modify-write   (ldadd, casal, ldxr/stxr)
     2. ordering / barriers        (ldar, stlr, dmb)
     3. ... and that is it.

   Read this file next to the note: every line is one of those three. */

#include <stdatomic.h>

int            plain;
atomic_int     counter;
atomic_int     flag;
atomic_llong   big;

/* --- 1. read-modify-write ------------------------------------------------- */

/* Not atomic: load, add, store. Three instructions, and another core can land
   between any two of them. This is a data race, which in C is undefined
   behaviour -- not "sometimes wrong", but *unbounded*. */
void inc_plain(void) { plain++; }

/* Atomic, relaxed: ONE instruction. `ldadd` is atomic add-and-return, so there
   is no window to lose. No ordering is implied, hence no barrier. */
void inc_relaxed(void) { atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed); }

/* Atomic, sequentially consistent (the default). Same operation plus full
   ordering: look for the acquire/release suffixes on the instruction. */
void inc_seqcst(void) { atomic_fetch_add(&counter, 1); }

/* --- 2. ordering: the same load, four times ------------------------------ */
int load_relaxed(void) { return atomic_load_explicit(&counter, memory_order_relaxed); }
int load_acquire(void) { return atomic_load_explicit(&counter, memory_order_acquire); }
int load_seqcst(void)  { return atomic_load(&counter); }

void store_relaxed(int v) { atomic_store_explicit(&counter, v, memory_order_relaxed); }
void store_release(int v) { atomic_store_explicit(&counter, v, memory_order_release); }
void store_seqcst(int v)  { atomic_store(&counter, v); }

/* A standalone fence, no data attached: this is the raw barrier instruction. */
void fence_acqrel(void) { atomic_thread_fence(memory_order_acq_rel); }
void fence_seqcst(void) { atomic_thread_fence(memory_order_seq_cst); }

/* --- 3. compare-and-swap: the universal primitive ----------------------- */

/* Everything else can be built from CAS, which is why hardware provides it.
   "Load the value, compute a new one, store it only if nobody else changed it,
   otherwise start over." */
int cas_once(int expected, int desired) {
    return atomic_compare_exchange_strong(&counter, &expected, desired);
}

/* The retry loop: this is what a lock-free algorithm looks like at the bottom. */
int atomic_max(int v) {
    int cur = atomic_load_explicit(&counter, memory_order_relaxed);
    while (v > cur) {
        if (atomic_compare_exchange_weak_explicit(&counter, &cur, v,
                memory_order_release, memory_order_relaxed))
            return 1;
        /* cur was updated with the current value; loop and try again */
    }
    return 0;
}

/* A spinlock, complete. Acquire must be an acquire, release must be a release,
   or the critical section can leak out of the lock in either direction. */
void lock(void) {
    while (atomic_exchange_explicit(&flag, 1, memory_order_acquire)) { /* spin */ }
}
void unlock(void) { atomic_store_explicit(&flag, 0, memory_order_release); }

/* --- is it lock-free? Ask, do not assume. ------------------------------- */
int is_lockfree_int(void)  { return atomic_is_lock_free(&counter); }
int is_lockfree_long(void) { return atomic_is_lock_free(&big); }
