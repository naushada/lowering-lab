# 06 — Objects, vtables, and the one indirection

**Claim:** objects need no runtime support at all. A method is a function taking
the object's address as argument 0. The *only* thing that needs machinery is
choosing which function at runtime, and that costs exactly one extra load and one
indirect branch.

```sh
make table
make run      # object sizes and base-subobject offsets
make dis      # demangled disassembly
```

## 1. `this` is x0, and that is the whole story

`p->get()` becomes `get(p)`. The mangled name `_ZNK5Plain3getEv` encodes the
class, the name, `const`, and the parameter types — which is all that C++
overloading and namespaces amount to: *a naming scheme*. Overload resolution,
access control, `public`/`private`: all resolved and discarded before codegen.
There is no `private` in the binary.

## 2. A virtual call is three instructions

```asm
call_virtual(Base const*):
    ldr  x8, [x0]        ; load the vptr (at offset 0 of the object)
    ldr  x1, [x8]        ; load slot 0 of the vtable = the function's address
    br   x1              ; indirect branch (a tail call, so br not blr)
```

That is polymorphism, complete. Every object of a polymorphic class carries a
pointer to a per-*class* (not per-object) table of function pointers. `f()` is
slot 0, `g()` is slot 1 — in `call_twice` you can see `ldr x8, [x8, #8]` picking
the second slot. The slot number is assigned at compile time; the *contents* are
what vary.

This is the same mechanism as the jump table in chapter 02, and as Go interfaces,
Rust `dyn Trait`, Java `invokevirtual`, and COM. "Table of function pointers plus
an indirection" is the universal answer to runtime choice.

## 3. The vptr is reloaded for the second call — on purpose

In `call_twice`, the vptr is loaded before `f()` and *again* before `g()`:

```asm
    ldr x8, [x0]  / ldr x8, [x8]     / blr x8      ; f()
    ldr x8, [x19] / ldr x8, [x8, #8] / blr x8      ; g()
```

It could not be cached, because C++ permits the callee to destroy the object and
construct a different type in the same storage. So a second dispatch really is a
second pair of loads. Aliasing rules are not pedantry; they are the reason a
redundant-looking load survives.

## 4. Devirtualisation: the cost is zero when the type is known

```
call_known_type()        2 insns    mov w0, #3    <- Derived d; d.f()
call_final(Final const*) 2 insns    mov w0, #42   <- `final` proves no override
call_local()             2 insns    mov w0, #42
```

No vtable, no call, not even arithmetic. When the dynamic type is provable, the
indirect call becomes direct, then inlines, then constant-folds. This is why
`final` is a performance annotation, why LTO matters for C++, and why "virtual
calls are slow" is only true when the compiler cannot see the type — which, in a
single translation unit with a local object, it usually can.

## 5. Inheritance is layout prefixing

```
A 4   B 8   C 12                     plain single inheritance: fields appended
VA 16  VB 16                         <- the vptr costs 8 bytes ONCE per object
LR 32   L at +0, R at +16
```

A `Derived` *begins with* a complete `Base`, which is precisely why a `Base*` may
point at it: the offsets of the base's fields are still correct. Upcasting is
free because it changes nothing.

Note `VA` and `VB` are both 16 bytes: `VA` is vptr(8) + int(4) + 4 padding, and
`VB`'s extra `int b` lands *in that padding*. A field for free.

## 6. Multiple inheritance is where the fiction shows

With `LR : L, R`, an `LR` object contains two base subobjects with **two vptrs**,
and `R` sits at offset 16. So a pointer conversion becomes arithmetic:

```asm
as_R(LR*):
    add  x8, x0, #16       ; R lives 16 bytes into an LR
    cmp  x0, #0
    csel x0, xzr, x8, eq   ; ...but nullptr must convert to nullptr, not to 0x10
as_L(LR*):
    ret                    ; L is at offset 0: genuinely free
```

And when someone calls `g()` through an `R*` that really points into an `LR`, the
`this` pointer arriving is the *R* subobject, 16 bytes too high for `LR::g`. So
the vtable entry does not point at `LR::g` — it points at a **thunk**:

```
non-virtual thunk to LR::g()        ; subtract 16 from this, then jump to LR::g
.quad  non-virtual thunk to LR::g()  <- what actually sits in R's vtable slot
```

`make table` on `layout.asm` also shows each destructor twice (`L::~L()` listed
twice): the complete object destructor and the deleting one. C++ has more entry
points than you wrote, and this is where the "C++ is only zero-overhead if you
don't use it" reputation comes from — single inheritance is genuinely free,
multiple inheritance costs an adjustment per call.

## Exercises

1. Add a third virtual function to `Base` and find its slot offset. Now add one
   to `Derived` only — where does it go, and why can it not share a slot number?
2. Remove `final` from `Final`. Watch `mov w0, #42` become three instructions.
3. Make `call_virtual` take `Base&` instead of `Base*`. Does the null check
   disappear? Should it?
4. Give `Derived` a second base with its own virtual function, then call it
   through the second base pointer and find the thunk in `make raw` output.
5. Add `-flto` to a two-file build where the derived class is in the other file.
   Does devirtualisation survive? This is the argument for LTO in one experiment.
