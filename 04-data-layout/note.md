# 04 — Data layout: types are a compile-time fiction

**Claim:** the machine has no structs, no arrays, no fields and no types. It has
addresses and sizes. Every field access is `base + constant`; every index is
`base + i*scale`. The names are gone before the first instruction is emitted.

```sh
make run     # sizes and offsets, printed
make table
```

## 1. Padding is not waste, it is the ABI

```
packed_well 16  (a@0 b@8 c@12)
padded      24  (a@0 b@8 c@16)  <- 14 bytes of holes
reordered   16  (b@0 a@8 c@9)   <- same three fields, sorted by size
```

`struct padded { char a; long b; char c; }` costs 24 bytes to hold 10 bytes of
data, because `long` must sit at an 8-byte boundary and the whole struct must be
a multiple of its strictest member's alignment (so that `arr[i]` keeps every
element aligned). Sort fields large-to-small and the same data fits in 16.

This matters more than it looks: 24 vs 16 bytes is the difference between 2 and 3
of these per 64-byte cache line.

## 2. A field access is an offset, and only an offset

```asm
_get_c:   ldr  w0, [x0, #12]     ; p->c
_nested:  ldr  w0, [x0, #8]      ; o->in.y  -- nesting just ADDS offsets
```

`p->c` and `*(int*)((char*)p + 12)` produce identical instructions, because they
*are* identical; the type system was the thing that knew `12` meant `c`. Nesting
is flattened at compile time — there is no pointer-chasing for `o.in.y`, just a
bigger constant.

## 3. Bitfields buy density with instructions

`get_bf` emits a shift and a mask; `set_bf` emits a load, mask, or, and store —
a **read-modify-write**. Two consequences: bitfields are not free, and two
threads writing two different bitfields in the same word will lose each other's
writes, because the hardware has no sub-byte store. (The C11 memory model
therefore does not treat adjacent bitfields as separate objects.)

## 4. A union is storage with two names and no memory of which

`float_bits` compiles to `fmov w0, s0` — a register move. The union did not
*do* anything; it just declined to reinterpret the bits. There is no tag, so
nothing at runtime knows which member is valid. That is what the enum next to it
in every real tagged union is for, and why `std::variant`/`Option` carry one.

## 5. Indexing: the scale is free unless it isn't

```asm
_idx_int:     ldr  w0, [x0, x1, lsl #2]     ; a + i*4,  one instruction
_idx_long:    ldr  x0, [x0, x1, lsl #3]     ; a + i*8,  one instruction
_idx_struct:  ... a real multiply by 12 ...  ; 12 is not a power of two
```

Power-of-two element sizes fold into the addressing mode and cost nothing.
`struct three { int a,b,c; }` is 12 bytes, so `a[i].b` needs an actual multiply.
This is one reason array-of-structs padded to 16 bytes can beat the packed
version despite using more memory.

## 6. 2D arrays are 1D with a multiply

`m[r][c]` on `int m[8][16]` becomes `r*16 + c`, i.e. `base + r*64 + c*4`. Nothing
about the declaration survives except the number 16. Row-major means the **last**
index is contiguous, which is the entire reason that swapping two nested loops
can change a program's speed by 10x without changing a single operation.

## 7. `sizeof` tells the truth; the signature lies

```c
long size_of_param(int a[10]) { return sizeof a; }   /* returns 8, not 40 */
```

The compiler warns about this (`-Wsizeof-array-argument`) — read the warning in
the build output, it is part of the lab. An array parameter is a pointer; `[10]`
is documentation. `manual()` in the same file does by hand what `a[i].b` does by
compilation, and produces the same instructions — proof there was never anything
else going on.

## Exercises

1. Reorder `padded`'s fields yourself until `make run` prints 16. Now add a
   `char d` — where does it go, and why is it free?
2. `__attribute__((packed))` on `padded`. Size drops; now look at what `get_b`
   compiles to. What did you trade?
3. Change `struct three` to four ints. Watch the multiply in `idx_struct` become
   a shift.
4. Write the 2D loop both ways (`m[r][c]` inner vs `m[c][r]` inner) over a large
   array, and time them. Explain the ratio using only chapter 04 and cache lines.
