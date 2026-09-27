# 07 — Closures: a struct and a function pointer

**Claim:** a closure is not a language primitive. It is *lambda lifting*: the body
is hoisted to an ordinary top-level function, and the captured variables are
packed into a struct passed as a hidden argument. It is the `void *userdata` of
every C callback API, generated for you.

```sh
make run      # closure sizes
make table
```

## 1. A closure's size is chapter 04 applied to captures

```
[] 1   [a] 4   [a,b] 16   [a,b,c] 24   [&a,&b] 16
```

- `[]` captures nothing, so there is nothing to carry. Size 1 only because C++
  forbids zero-sized objects, and a no-capture lambda therefore converts to a
  plain `int(*)(int)` — `no_capture()` compiles to 2 instructions.
- `[a, b]` with `int a; long b;` is 16 bytes, not 12: the `long` needs 8-byte
  alignment, so there is a 4-byte hole. **A closure is a struct, padding
  included.** You can make a lambda smaller by capturing in a better order.
- `[&a, &b]` is two pointers. Capturing by reference costs a pointer per
  variable and copies nothing — which is why it is cheap, and why it is dangerous.

## 2. When the type is concrete, the closure disappears entirely

```
no_capture(int)        2 insns
by_value(int, int)     3 insns
by_reference(int, int) 3 insns
call_direct(int)       2 insns     ->  add w0, w0, #1
```

The struct was never built, nothing was copied, there was no call. A lambda whose
type the compiler knows is a *compile-time* construct: it exists to organise your
source, and is gone by codegen. This is the honest meaning of "zero-cost".

## 3. `dangling()` — the bug that no instruction set could prevent

```cpp
auto dangling() { int local = 42; return [&local]{ return local; }; }
```

The closure stores `&local`, and `local`'s frame is released on return. Read the
warning in the build output (`-Wreturn-stack-address`) — it is part of the lab.
The captured *lifetime* is not represented anywhere in the machine; a reference
capture is a pointer, and a pointer has no idea what it points at. Languages that
make this impossible do so entirely in the front end: Rust's borrow checker, or
Java/Go/C# forcing capture by value and moving the variable to the heap.

## 4. Type erasure: `std::function` is a different mechanism

`std::function<int(int)>` is 32 bytes and accepts *any* callable with that
signature, so its size cannot depend on the captures and the call cannot be
direct. It is a vtable-based box. `make table` on `erased.asm` and count what one
`std::function` over one lambda dragged into the object file:

```
__func<lambda>::~__func()              __func<lambda>::destroy()
__func<lambda>::__clone() x2           __func<lambda>::destroy_deallocate()
__func<lambda>::operator()(int&&)      __func<lambda>::target(type_info const&)
__func<lambda>::target_type() const
```

That is the erasure machinery: clone, destroy, deallocate, invoke, identify. Even
when the *call* optimises away, this code is emitted — abstraction cost shows up
as code volume, not only as cycles.

## 5. The A/B test: erased vs monomorphised

Same lambda, same arithmetic, called through a boundary the optimiser cannot see
through (`noinline`):

```asm
invoke_erased(std::function<int(int)> const&, int):     ; 13 instructions
    stur w1, [x29, #-4]      ; the int must go to MEMORY: the erased signature
    ldr  x0, [x0, #24]       ;   takes int&&, so it needs an address
    cbz  x0, LBB3_2          ; empty std::function -> throw bad_function_call
    ldr  x8, [x0]            ; the box's vptr
    ldr  x8, [x8, #48]       ; slot 6 = operator()
    blr  x8                  ; indirect call

int invoke_tmpl<lambda>(lambda, int):                   ; 2 instructions
    add  w0, w0, #1
    ret
```

13 instructions against 2, for identical semantics. The costs are: an indirection,
a null check, a heap-or-buffer decision, and the argument forced into memory. In
exchange you get one function that works for every callable instead of a fresh
copy per callable — which is exactly the trade of chapter 08.

`call_erased_big` captures 48 bytes of `long`s, overflowing `std::function`'s
small-object buffer, and pulls in `operator new`. A closure that *allocates*: the
thing people mean when they say lambdas are free and are sometimes wrong.

## 6. `call_manual` — the same thing, by hand, in C

```c
struct env { int x; };
static int adder(void *ud, int v) { return ((struct env *)ud)->x + v; }
```

Two instructions, identical to the lambda. Every C API that takes
`(callback, void *userdata)` — `pthread_create`, `qsort_r`, every event loop — is
a hand-written closure. The language feature added type safety and inference, not
capability.

## Exercises

1. Reorder `[a, b, c]`'s captures by size and get `sizeof` down. How much can you
   save?
2. Change `by_reference` to capture `[=]` and then `[&]`. Which one still shows
   `acc` living in memory, and why?
3. Make `via_erased` pass the `std::function` *by value* instead of by reference.
   Count the extra copy/destroy calls.
4. Replace `std::function` with `std::function_ref`-style hand-rolled
   `{void* env; int(*fn)(void*,int);}` (16 bytes, no allocation, no clone). How
   close to `invoke_tmpl` can you get while staying non-templated?
5. Return the lambda from `by_value` (by value) instead of calling it. Where does
   the struct live now?
