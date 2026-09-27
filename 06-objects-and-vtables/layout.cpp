// Inheritance is layout prefixing, and that is the whole reason a Base* can
// point at a Derived: a Derived *begins with* a complete Base.
#include <cstdio>

struct A { int a; };
struct B : A { int b; };
struct C : B { int c; };          // one int after another: 4, 8, 12 bytes

struct VA { int a; virtual void f(); };          // + 8 bytes for the vptr
struct VB : VA { int b; };
void VA::f() {}

// Multiple inheritance: an object now contains TWO base subobjects at different
// offsets, so a pointer conversion becomes pointer ARITHMETIC, and calling
// L2::f() through an R* needs a thunk to fix `this` back up.
struct L { int l; virtual void f(); virtual ~L() = default; };
struct R { int r; virtual void g(); virtual ~R() = default; };
struct LR : L, R { int both; void f() override; void g() override; };
void L::f() {} void R::g() {} void LR::f() {} void LR::g() {}

R *as_R(LR *p) { return p; }       // NOT a no-op: adds the offset of R inside LR
L *as_L(LR *p) { return p; }       // a no-op: L is at offset 0

int main() {
    printf("A %zu  B %zu  C %zu\n", sizeof(A), sizeof(B), sizeof(C));
    printf("VA %zu  VB %zu   <- the vptr costs 8 bytes, once, per object\n",
           sizeof(VA), sizeof(VB));
    LR obj{};
    printf("LR %zu   L at +%ld, R at +%ld  <- two vptrs in one object\n",
           sizeof(LR),
           (long)((char *)static_cast<L *>(&obj) - (char *)&obj),
           (long)((char *)static_cast<R *>(&obj) - (char *)&obj));
    return 0;
}
