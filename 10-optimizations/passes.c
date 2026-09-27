/* Lowering gets you correct code. Optimisation passes are what make it fast, and
   each one is a rewrite you could do by hand but should not.
   `make compare` is the point of this file: -O0 next to -O2. */

#include <string.h>

/* --- constant folding and propagation: arithmetic done by the compiler ---- */
int fold(void) {
    int a = 6, b = 7;
    int c = a * b;
    return c + (1 << 4) - 8;         /* one `mov` at -O1 */
}

/* --- inlining, then everything else. Inlining is the ENABLING pass: it does
   not just remove a call, it removes the wall that stopped other passes. ---- */
static inline int square(int x) { return x * x; }
int sum_of_squares(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += square(i);
    return s;                         /* -O2: no call, no loop -- a CLOSED FORM */
}

/* --- strength reduction: division by a constant is a multiply by a magic
   number and a shift, because dividing is ~20x slower than multiplying. ----- */
int div_by_7(int x)  { return x / 7; }
int div_by_8(int x)  { return x / 8; }     /* power of two: just shifts */
int mod_by_10(int x) { return x % 10; }

/* --- common subexpression elimination: compute f(a,b) once, not twice ----- */
int cse(int a, int b) {
    int p = (a + b) * (a + b);
    int q = (a + b) - 3;
    return p + q;
}

/* --- loop-invariant code motion: `k*k` does not change, so hoist it out --- */
int licm(const int *a, int n, int k) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i] * (k * k);
    return s;
}

/* --- idiom recognition: the compiler pattern-matches your loop against a
   library function or a single instruction. It matches SHAPES, so two correct
   versions of the same function can get very different treatment. --------- */
void copy_loop(char *d, const char *s, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    /* -O0/-O1/-Os: a plain byte loop, 6 instructions.
       -O2: VECTORISED into 32-byte chunks (ldp q0,q1 / stp q0,q1) with an
       overlap guard and two remainder loops -- 44 instructions. Optimised code
       is bigger here, and -Os deliberately declines the trade. */
}

/* These three compute the same thing. Two of them compile to the same five
   instructions using the `cnt` instruction; one stays a loop forever. */
int popcount_shift(unsigned x) {
    int n = 0;
    while (x) { n += x & 1; x >>= 1; }        /* NOT recognised: stays a loop */
    return n;
}
int popcount_kernighan(unsigned x) {
    int n = 0;
    while (x) { x &= x - 1; n++; }            /* recognised -> cnt */
    return n;
}
int popcount_builtin(unsigned x) { return __builtin_popcount(x); }

/* --- dead code elimination: no observable effect, therefore no code ------- */
int dead(int x) {
    int unused = x * 12345;
    for (int i = 0; i < 1000; i++) { int t = i * i; (void)t; }
    (void)unused;
    return x;
}

/* --- tail duplication / jump threading: the branch is resolved at compile
   time along one path, so the test disappears on that path. -------------- */
int threaded(int x) {
    int flag = (x > 10);
    if (flag) x += 1;
    if (flag) x += 2;          /* the second test is the same as the first */
    return x;
}

/* --- what the compiler may NOT do: floating point is not associative, so this
   loop cannot be reassociated or vectorised without -ffast-math. --------- */
float fsum(const float *a, int n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
