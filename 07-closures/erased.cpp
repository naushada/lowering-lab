// std::function is a different animal from a lambda: it accepts ANY callable of
// the right signature, so its size is fixed and the call must be indirect.
// That is type erasure, and it costs a vtable-shaped indirection plus, when the
// captures do not fit in the small-buffer, a heap allocation.
#include <functional>
#include <cstdio>

int call_direct(int x) {
    auto f = [x](int v) { return v + x; };      // concrete type, inlinable
    return f(1);
}

int call_erased(int x) {
    std::function<int(int)> f = [x](int v) { return v + x; };
    return f(1);                                 // indirect: blr through a pointer
}

// Captures too big for std::function's internal buffer force an allocation.
// Look for a call to operator new in the -O1 output of this one.
int call_erased_big(int x) {
    long a=x, b=x+1, c=x+2, d=x+3, e=x+4, g=x+5;
    std::function<long()> f = [a,b,c,d,e,g] { return a+b+c+d+e+g; };
    return (int)f();
}

// To see the indirection itself, the call must cross a boundary the compiler
// cannot look through. These two are the A/B test of the whole chapter:
//   invoke_erased  takes a std::function  -> one size fits all, INDIRECT call
//   invoke_tmpl    is a template          -> a fresh copy per callable, DIRECT call
__attribute__((noinline))
int invoke_erased(const std::function<int(int)> &f, int v) { return f(v); }

template <class F>
__attribute__((noinline))
int invoke_tmpl(F f, int v) { return f(v); }

int via_erased(int x) { return invoke_erased([x](int v) { return v + x; }, 1); }
int via_tmpl(int x)   { return invoke_tmpl  ([x](int v) { return v + x; }, 1); }

// The C way, which is the same mechanism written out longhand: a function
// pointer plus a void* environment. Every callback API in C is a manual closure.
struct env { int x; };
static int adder(void *ud, int v) { return ((struct env *)ud)->x + v; }
int call_manual(int x) {
    struct env e = { x };
    int (*fp)(void *, int) = adder;
    return fp(&e, 1);
}

int main() {
    printf("sizeof(std::function<int(int)>) = %zu\n", sizeof(std::function<int(int)>));
    printf("%d %d %d %d %d %d\n", call_direct(1), call_erased(1), call_erased_big(1),
           call_manual(1), via_erased(1), via_tmpl(1));
}
