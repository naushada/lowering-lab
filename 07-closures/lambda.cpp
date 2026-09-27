// A closure is a struct plus a function. That is all. The compiler hoists the
// body out to a top-level function ("lambda lifting") and passes the captured
// variables to it as a hidden argument -- exactly the `void *userdata` you would
// have written by hand in C.
#include <cstdio>

int apply_fp(int (*f)(int), int x) { return f(x); }

// No captures -> there is nothing to carry, so the lambda converts to a plain
// function pointer. sizeof is 1 only because C++ forbids zero-sized objects.
int no_capture(int x) {
    auto f = [](int v) { return v * 2; };
    return apply_fp(f, x);                  // implicit conversion to int(*)(int)
}

// Capture by value -> the values are COPIED into the closure object, which is a
// struct laid out like any other (chapter 04 applies: padding and all).
int by_value(int a, int b) {
    auto f = [a, b](int v) { return v * a + b; };
    return f(1) + f(2);                     // inlined: watch the copies vanish
}

// Capture by reference -> the closure stores ADDRESSES. Cheap to make, and the
// reason a closure can outlive what it points at (see dangling() below).
int by_reference(int a, int b) {
    int acc = 0;
    auto f = [&acc, a](int v) { acc += v * a; };
    f(1); f(2); f(3);
    return acc + b;
}

// A capturing lambda is an OBJECT, so it has a size you can print.
void sizes() {
    int a = 1; long b = 2; char c = 3;
    auto l0 = []          { return 0; };
    auto l1 = [a]         { return a; };
    auto l2 = [a, b]      { return a + (int)b; };
    auto l3 = [a, b, c]   { return a + (int)b + c; };
    auto l4 = [&a, &b]    { return a + (int)b; };
    printf("[] %zu  [a] %zu  [a,b] %zu  [a,b,c] %zu  [&a,&b] %zu\n",
           sizeof l0, sizeof l1, sizeof l2, sizeof l3, sizeof l4);
}

// THE classic bug, visible as an address: the closure holds &local, and the
// frame holding `local` is gone by the time anyone calls it. Nothing in the
// instruction set could have prevented this; the lifetime was never represented.
auto dangling() {
    int local = 42;
    return [&local] { return local; };   // -O1 will warn; read the warning
}
int use_dangling() { return dangling()(); }

int main() { sizes(); printf("%d %d %d\n", no_capture(5), by_value(2,3), by_reference(2,3)); }
