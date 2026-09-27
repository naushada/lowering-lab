// MONOMORPHISATION (C++ templates, Rust generics, D, Zig): emit a separate,
// fully specialised copy of the code for every type it is used with. The generic
// code does not exist at runtime -- only the copies do.
//
// After `make dis`, count the copies for real:
//     nm -gU out/O1/mono.o | c++filt | grep sum_
#include <cstdio>

template <class T>
__attribute__((noinline))                 // keep the copies visible
T sum(const T *a, int n) {
    T s{};
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

struct Money { long cents; void operator+=(const Money &o) { cents += o.cents; } };

// Three uses of ONE template -> three functions in the object file, each with a
// different element size, a different add instruction, and different addressing.
int   use_int   (const int *a, int n)   { return sum<int>(a, n); }        // add,  lsl #2
long  use_long  (const long *a, int n)  { return sum<long>(a, n); }       // add,  lsl #3
double use_double(const double *a, int n){ return sum<double>(a, n); }    // fadd, lsl #3
long  use_money (const Money *a, int n) { return sum<Money>(a, n).cents; }

// A typedef is NOT a new type, so this adds no fifth copy -- it reuses
// sum<long>. Aliases are transparent all the way down.
// (The real code-duplication case is sum<Money> vs sum<long>: two symbols whose
// instructions are identical apart from label names. See the note.)
using Cents = long;
long use_cents(const Cents *a, int n) { return sum<Cents>(a, n); }

int main() {
    int   ai[4] = {1,2,3,4};
    double ad[4] = {1.5,2.5,3.5,4.5};
    printf("%d %.1f\n", use_int(ai,4), use_double(ad,4));
}
