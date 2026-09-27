# 08 — Generics: one copy per type, or one copy for all types

**Claim:** there are exactly two ways to compile "code that works for many
types", and every language picks one (or offers both):

1. **Monomorphisation** — emit a specialised copy per type. Fast, big, slow to
   compile. C++ templates, Rust, Zig, D, .NET value-type generics.
2. **Type erasure** — emit one copy that manipulates values through a uniform
   representation (a pointer, plus a size or a vtable). Small, slower, fast to
   compile. Java, Go interfaces, C `void *`, OCaml, .NET reference generics.

```sh
make table
make dis && nm -gU out/O1/mono.o | c++filt | grep sum
```

## 1. Monomorphisation: the generic function does not exist

```
int    sum<int>(int const*, int)        13 insns
long   sum<long>(long const*, int)      13 insns
double sum<double>(double const*, int)  11 insns     <- fadd, not add
Money  sum<Money>(Money const*, int)    13 insns
```

One template, four functions in the object file, confirmed by `nm`. Each is
fully specialised: `sum<int>` uses `add` and `lsl #2`, `sum<double>` uses `fadd`
and `lsl #3`, and `sum<Money>` inlined the user-defined `operator+=` until
nothing of the abstraction remained. And each caller is **one instruction** — a
tail call — because there was nothing left to do but jump.

There is no `sum` in the binary. There never was: a template is a *code
generator* that runs in the compiler.

## 2. The cost, measured

Diff the bodies of `sum<long>` and `sum<Money>`, ignoring label names:

```
2c2
<   b.lt LBB3_4
>   b.lt LBB7_4
```

**Identical machine code under two symbols.** `Money` is a `long` in a struct, so
the instructions must be the same — but the compiler reasons about types, not
about instructions, so it emits both. Multiply that by every container of every
type in a large C++ program and you have the classic template code-size problem,
along with the compile times. (Linkers can fold identical functions —
`-Wl,-dead_strip`, or ICF on other platforms — which is a fix applied *after* the
bloat is created.)

Meanwhile `use_cents` with `using Cents = long` adds **no** fifth copy: an alias
is transparent, and only genuinely distinct types instantiate.

## 3. Erasure: one copy, and the type information becomes arguments

`accumulate()` in `erased.c` is the same algorithm with everything static made
dynamic:

```
_accumulate   23 insns   1 call   SAVED x19..x24
```

- the element size is a *parameter*, so advancing the pointer is a runtime add
  rather than a folded `lsl #2`;
- the operation is a *function pointer*, so there is an **indirect call per
  element** and it cannot be inlined, vectorised, or reassociated;
- six callee-saved registers, because the loop state must survive that call
  (chapter 05).

One function serves every type. `qsort` versus `std::sort` is this exact trade,
and it is the usual explanation for the several-fold difference between them.

## 4. But erasure can be undone — read this carefully

```
_use_int_erased      12 insns   0 calls
_use_double_erased    8 insns   0 calls
```

Zero calls. The compiler inlined `accumulate`, saw that `elem_size` was `4` and
that `add` was a `static` function it could see, replaced the indirect call with a
direct one, inlined that too, and produced a loop as tight as `sum<int>`.

So erasure costs nothing *when the compiler can see through it in the same
translation unit*. The cost is real when it cannot: across a library boundary,
behind a `.so`, or through a function pointer that genuinely varies at runtime.
This is the same lesson as devirtualisation in chapter 06, and the reason
profile-guided optimisation bothers with indirect-call promotion.

## 5. Boxing: the third strategy

`sum_boxed` walks an array of *pointers* to heap objects: two loads per element
instead of one, no contiguity, and an allocation per value. That is how Java
`List<Integer>` and every dynamic language represent a collection, and it is why
`int[]` and `Integer[]` have completely different performance despite looking
alike. There is only ever one copy of the code, because there is only ever one
representation: a pointer.

## Exercises

1. Add `sum<short>` and `sum<char>`. Does either reuse an existing body? Check
   with `nm`.
2. Move `add_int` out of `erased.c` into a second file so it cannot be inlined.
   Rebuild and count the indirect calls that come back.
3. Compile `mono.cpp` with `-Os`. Do the duplicate bodies get merged? Now try
   linking with `-Wl,-dead_strip` and compare object vs binary sizes.
4. Write the erased version so the *whole loop* is the callback's job
   (`add_all(void*, const void*, int)`) — one indirect call instead of n. How
   close to the monomorphised version does that get, and what did you give up?
5. Rust comparison, if you have it: `fn sum<T: Add>(...)` vs `&dyn Fn`. Predict
   which is which in `cargo asm` before you look.
