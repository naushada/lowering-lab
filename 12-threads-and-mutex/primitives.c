/* Here is the surprise of this chapter: the compiler does almost NOTHING for
   threads. There is no thread instruction, no parallel construct, no scheduler.
   Read the assembly -- every line below is an ordinary function call.

   Threads are built from three layers:
     HARDWARE  multiple cores, each with its own registers and program counter,
               plus the atomics of chapter 11 and a timer interrupt.
     KERNEL    a "thread" is a saved register set + a stack; a context switch is
               an interrupt handler storing one and loading another.
     LIBRARY   pthreads/libc wraps the syscalls; the language calls the library.
   The compiler's only real job is to stop optimising as if it were alone. */

#include <pthread.h>
#include <stdatomic.h>

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t  once_flag = PTHREAD_ONCE_INIT;
static _Thread_local int tls_counter;       /* thread_local in C++ */
static atomic_int spin;

void *worker(void *arg);

/* A thread is created by ASKING THE KERNEL. `bl _pthread_create` and that is
   the entire lowering -- the compiler has no idea what a thread is. */
int spawn(pthread_t *t, void *arg) {
    return pthread_create(t, NULL, worker, arg);
}
int reap(pthread_t t, void **out) { return pthread_join(t, out); }

/* A mutex is not an instruction either. It is a library call that, on its fast
   path, does a CAS in user space (chapter 11) and only enters the kernel when it
   actually has to block. You cannot see that split from here: it is inside libc. */
void critical_section(int *p) {
    pthread_mutex_lock(&mtx);
    (*p)++;                                  /* an ordinary, non-atomic ++ */
    pthread_mutex_unlock(&mtx);
}

/* The hand-rolled equivalent, so you can see the fast path that libc hides.
   This is what pthread_mutex_lock does FIRST, before deciding to sleep. */
void critical_section_spin(int *p) {
    while (atomic_exchange_explicit(&spin, 1, memory_order_acquire)) { }
    (*p)++;
    atomic_store_explicit(&spin, 0, memory_order_release);
}

/* Thread-local storage: each thread needs a different address for the same name,
   so the address cannot be a link-time constant. Look for the indirect call --
   on Darwin, through a "thread-local variable" descriptor. */
int tls_get(void) { return tls_counter; }
void tls_inc(void) { tls_counter++; }

/* Lazy init, done right: one call, and the library guarantees exactly-once
   across all threads. Doing this yourself with a bool is the double-checked
   locking bug. */
static void init_once(void) { }
void ensure_init(void) { pthread_once(&once_flag, init_once); }
