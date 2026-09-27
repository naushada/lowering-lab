/* TYPE ERASURE, the C version: ONE copy of the code, parameterised at RUNTIME by
   a size and a function pointer. This is how qsort, and Java's generics, and
   Go's pre-1.18 interface containers all work.

   Compare the loop here with any one of mono.cpp's copies. The algorithm is the
   same; everything the compiler knew statically has become a runtime argument. */

#include <string.h>

void accumulate(void *acc, const void *arr, int n, unsigned long elem_size,
                void (*add)(void *, const void *)) {
    const char *p = (const char *)arr;
    for (int i = 0; i < n; i++) {
        add(acc, p);            /* an INDIRECT CALL per element: cannot inline */
        p += elem_size;         /* a runtime multiply-add, not lsl #2         */
    }
}

static void add_int(void *a, const void *b)    { *(int *)a += *(const int *)b; }
static void add_double(void *a, const void *b)  { *(double *)a += *(const double *)b; }

int use_int_erased(const int *a, int n) {
    int acc = 0;
    accumulate(&acc, a, n, sizeof(int), add_int);
    return acc;
}
double use_double_erased(const double *a, int n) {
    double acc = 0;
    accumulate(&acc, a, n, sizeof(double), add_double);
    return acc;
}

/* The third strategy, used by Java for Object generics and by every dynamic
   language: make everything a pointer to a heap box, so there is exactly one
   representation and therefore exactly one copy of the code. The cost is a
   dereference per access and an allocation per value. */
struct box { long v; };
long sum_boxed(struct box *const *a, int n) {
    long s = 0;
    for (int i = 0; i < n; i++) s += a[i]->v;   /* two loads per element */
    return s;
}
