/* A data race, measured. Four threads, each adding 1 a million times, four ways.
   `make run` -- and run it more than once, because that is the point. */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define THREADS 4
#define ITERS   1000000

static long            plain_counter;
static atomic_long     relaxed_counter;
static atomic_long     seqcst_counter;
static long            mutex_counter;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void *run_plain(void *_)   { for (int i = 0; i < ITERS; i++) plain_counter++; return 0; }
static void *run_relaxed(void *_) { for (int i = 0; i < ITERS; i++) atomic_fetch_add_explicit(&relaxed_counter, 1, memory_order_relaxed); return 0; }
static void *run_seqcst(void *_)  { for (int i = 0; i < ITERS; i++) atomic_fetch_add(&seqcst_counter, 1); return 0; }
static void *run_mutex(void *_)   { for (int i = 0; i < ITERS; i++) { pthread_mutex_lock(&mtx); mutex_counter++; pthread_mutex_unlock(&mtx); } return 0; }

static double bench(void *(*fn)(void *)) {
    struct timespec a, b;
    pthread_t t[THREADS];
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, fn, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

int main(void) {
    long want = (long)THREADS * ITERS;
    double t_plain   = bench(run_plain);
    double t_relaxed = bench(run_relaxed);
    double t_seqcst  = bench(run_seqcst);
    double t_mutex   = bench(run_mutex);

    printf("expected           %9ld\n", want);
    printf("plain   ++         %9ld  %7.1f ms   %s\n", plain_counter, t_plain,
           plain_counter == want ? "(correct THIS TIME -- it is still a race)" : "<- updates LOST");
    printf("atomic  relaxed    %9ld  %7.1f ms\n", (long)relaxed_counter, t_relaxed);
    printf("atomic  seq_cst    %9ld  %7.1f ms\n", (long)seqcst_counter, t_seqcst);
    printf("mutex              %9ld  %7.1f ms\n", mutex_counter, t_mutex);
    return 0;
}
