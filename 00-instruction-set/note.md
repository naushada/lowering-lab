# 00 — The instruction set, one instruction at a time

The rest of this repo shows how language constructs become instructions. This
chapter is the reference for the instructions themselves: what each one does, and
**the C that provokes it**, so you can check every claim here yourself.

```sh
make                 # out/O1/integer.asm and out/O1/float_simd.asm
make mnemonics       # every distinct instruction emitted, with counts
make OPT=O2 mnemonics  # ...and the vector ones that only appear at -O2
```

90 distinct instructions come out of these two files. That is essentially the
whole working vocabulary of the machine.

---

## 1. The registers

**Integer file — 31 general registers plus a zero register.** Each is 64 bits,
and every one has a 32-bit alias. `x3` and `w3` are *the same register*: `w3` is
its low half. Writing `w3` zeroes the top half; that is why narrowing a `long` to
an `int` costs nothing.

| Register | Role (set by the ABI, not the hardware) |
|---|---|
| `x0`–`x7` | arguments 1–8; `x0` also carries the return value |
| `x8` | address of a returned struct; otherwise scratch |
| `x9`–`x15` | scratch, caller-saved — a callee may destroy these |
| `x16`, `x17` | reserved for the linker (veneers) |
| `x18` | reserved by the platform |
| `x19`–`x28` | **callee-saved**: seeing these in a prologue means a value had to survive a call (chapter 05) |
| `x29` / `fp` | frame pointer |
| `x30` / `lr` | link register: `bl` writes the return address here |
| `sp` | stack pointer; must be 16-byte aligned at every call |
| `xzr` / `wzr` | the zero register: reads as 0, writes are discarded |

For diagrams of how `x0` and `w0` overlap — and why there is no name for bits
63..32 on either arm64 or x86-64 — see
[docs/register-views.md](../docs/register-views.md), and run `make run` in this
chapter to watch a `w` write zero the upper half.

`xzr` is how the machine avoids needing extra instructions: `neg x0, x1` is
really `sub x0, xzr, x1`, `cmp` is `subs` into `xzr`, and `mov x0, x1` is
`orr x0, xzr, x1`.

**Floating-point / vector file — 32 separate registers**, `v0`–`v31`, named by
how much of each you are using:

| Name | Width | Meaning |
|---|---|---|
| `b0` `h0` `s0` `d0` | 8 / 16 / 32 / 64 bits | scalar: `s` = `float`, `d` = `double` |
| `q0` | 128 bits | the whole vector register |
| `v0.4s` | 4 lanes × 32 bits | four floats or four ints at once |
| `v0.2d` `v0.8b` `v0.16b` | 2×64 / 8×8 / 16×8 | other lane arrangements |

`v0`–`v7` pass floating-point arguments, exactly parallel to `x0`–`x7`. The two
files are separate, so integer and float code do not compete for registers — you
can see this in chapter 05's exercise 4.

## 2. The flags, and the condition codes

Four bits, set by any instruction ending in `s` (and by `cmp`, `cmn`, `tst`):

| Flag | Name | Set when |
|---|---|---|
| `N` | Negative | the result's top bit is 1 |
| `Z` | Zero | the result was zero |
| `C` | Carry | unsigned overflow (or "no borrow" on a subtract) |
| `V` | oVerflow | signed overflow |

Every conditional instruction names one of these combinations:

| Code | Meaning | Signed? | Code | Meaning |
|---|---|---|---|---|
| `eq` | equal (`Z=1`) | — | `ne` | not equal |
| `lt` | less than | signed | `ge` | greater or equal (signed) |
| `gt` | greater than | signed | `le` | less or equal (signed) |
| `lo` | lower | **unsigned** | `hs` | higher or same (unsigned) |
| `hi` | higher | **unsigned** | `ls` | lower or same (unsigned) |
| `mi` | minus (`N=1`) | — | `pl` | plus |
| `vs` | overflow set | — | `vc` | overflow clear |

`lt`/`ge` versus `lo`/`hs` is the signedness of your *types*, visible in the
instruction. When you see `b.lo` you are looking at unsigned arithmetic — which
is how chapter 02's range check works: `sub` the low bound, then one **unsigned**
compare catches "below the range" as a huge positive number.

## 3. Constants: why `movk` exists

Every instruction is exactly 32 bits, so a 64-bit constant cannot fit inside one.
Constants are built 16 bits at a time:

```asm
; long const_small(void)  { return 42; }
    mov  x0, #42                        ; one instruction

; long const_32bit(void)  { return 0x12345678; }
    mov  w0, #22136                     ; movz: low half, zero the rest
    movk w0, #4660, lsl #16             ; movk: keep the rest, patch bits 16-31

; long const_64bit(void) { return 0x123456789ABCDEF0L; }
    mov  x0, #57072
    movk x0, #39612, lsl #16
    movk x0, #22136, lsl #32
    movk x0, #4660,  lsl #48            ; four instructions for one number
```

- `movz` — **move and zero** the other bits (the assembler prints it as `mov`)
- `movk` — **move and keep** the other bits
- `mvn` — move-not: `~x`, also used to make some negative constants in one go

**The `lsl #16` here does not shift `x0`.** It applies to the 16-bit immediate and
selects which slot to write; the bits already in `x0` do not move. The assembler
accepts only `lsl #0`, `#16`, `#32` and `#48` — `#8` is rejected — because the
field is 2 bits wide and names a slot rather than a distance. Full explanation and
the live trace in [docs/register-views.md](../docs/register-views.md).

This is why `return -1` is one instruction (`mov x0, #-1`) while
`return 0x12345678` is two.

`add` has its own, different immediate field: **12 bits, optionally shifted left
by 12.** So the boundaries are:

```asm
x + 0xFFF       add w0, w0, #4095                          ; 1 insn
x + 0x1000      add w0, w0, #1, lsl #12                    ; 1 insn
x + 0x123456    add w8, w0, #291, lsl #12                   ; 2 insns: two 12-bit
                add w0, w8, #1110                           ;   chunks
x + 0x12345678  mov w8, #22136 / movk w8, #4660, lsl #16    ; 3 insns: build it
                add w0, w0, w8                              ;   in a register first
```

Compare `add_small_imm` with `add_large_imm` in the output to see the two-add
form. This is the general shape of every immediate field on the machine: each
instruction encodes the constants *it* expects to need, and the assembler quietly
synthesises anything larger.

## 4. Data movement

| Instruction | Does | Provoked by |
|---|---|---|
| `mov w0, w1` | register copy | `move_reg` |
| `mov w0, #42` | small immediate | `const_small` |
| `movk` | patch 16 bits, keep the rest | `const_32bit`, `const_64bit` |
| `mvn w0, w1` | `w0 = ~w1` | `bitwise_not` |
| `neg w0, w1` | `w0 = -w1` (= `sub w0, wzr, w1`) | `negate` |
| `fmov w0, s0` | copy bits between register files, no conversion | `float_bits` |
| `adrp`+`add` | form the address of a global | `address_of_global` |

`adrp x8, sym@PAGE` / `add x8, x8, sym@PAGEOFF` is worth understanding: a single
instruction cannot hold a 64-bit address, so the linker splits it into a 4 KB
page address and an offset within the page. **Taking the address of a global is
two instructions**, which is why chapter 11's atomics all begin with this pair.

## 5. Arithmetic

| Instruction | Does | Provoked by |
|---|---|---|
| `add` / `sub` | add / subtract (register, or a 12-bit immediate optionally shifted by 12) | `add_small_imm`, `add_large_imm` |
| `adds` / `subs` | the same, **and set the flags** | `add128`, loop latches |
| `cmp w0, w1` | `subs wzr, w0, w1` — flags only | `compare` |
| `cmn w0, #5` | `adds wzr, w0, #5` — compare against a *negative* | `branch_if_neg_const` |
| `adc` / `sbc` | add / subtract **with carry**: multi-word arithmetic | `add128` |

**Free shifts and extensions.** The second operand can be shifted or extended in
the same instruction, at no cost:

```asm
; int add_shifted(int x, int y) { return x + (y << 3); }
    add w0, w0, w1, lsl #3         ; ONE instruction

; long add_extended(long x, int y) { return x + y; }
    add x0, x0, w1, sxtw           ; sign-extend w1 to 64 bits, then add
```

This is the single most important thing to know when counting instructions: `a +
b*8`, `p[i]` and `base + index*scale` are all *one* instruction. It is also why
multiplying by a power of two never shows a `mul`.

**Carry, made visible.** `add128` is the only way to see `adc` from portable C:

```asm
    adds x0, x0, x2        ; low half, setting carry
    adc  x1, x1, x3        ; high half, plus the carry
```

## 6. Multiply and divide

| Instruction | Does | Provoked by |
|---|---|---|
| `mul w0, w1, w2` | `w1 * w2` (low half) | `multiply` |
| `madd w0, w1, w2, w3` | `w1*w2 + w3`, one instruction | `mul_add` |
| `msub w0, w1, w2, w3` | `w3 - w1*w2` | `mul_sub` |
| `smull x0, w1, w2` | signed 32×32 → **64-bit** result | `widening_signed` |
| `umull x0, w1, w2` | unsigned 32×32 → 64 | `widening_unsigned` |
| `umulh x0, x1, x2` | the **high** 64 bits of a 64×64 multiply | `multiply_high` |
| `sdiv` / `udiv` | signed / unsigned divide | `divide_signed` |

Two things that surprise people:

- **`madd` means `a*b + c` is never two instructions.** Neither is `c - a*b`.
- **There is no remainder instruction.** `a % b` is `sdiv` followed by `msub`:

```asm
; int modulo(int a, int b) { return a % b; }
    sdiv w8, w0, w1        ; q = a / b
    msub w0, w8, w1, w0    ; a - q*b   <- the remainder
```

Division is the slowest integer operation on the chip (~20 cycles against 3 for
a multiply), which is the entire reason chapter 10's `div_by_7` becomes a
multiply by a magic reciprocal. `umulh` is the instruction that makes that
trick possible.

## 7. Logic, shifts, bits

| Instruction | Does | Provoked by |
|---|---|---|
| `and` / `orr` / `eor` | bitwise and / or / xor | `bit_and`, `bit_or`, `bit_xor` |
| `bic w0, w1, w2` | **bit clear**: `w1 & ~w2`, one instruction | `bit_clear` |
| `tst w0, w1` | `ands wzr, w0, w1` — test bits, flags only | `branch_if_mask` |
| `lsl` / `lsr` | shift left / right **logical** (fills with 0) | `shift_left` |
| `asr` | shift right **arithmetic** (fills with the sign bit) | `shift_right_arith` |
| `ror` | rotate right | `rotate_right` |

`lsl` and `lsr` mean exactly what `<<` and `>>` mean in C — left is toward the
most significant bit, and the vacated bits are filled with zeros:

```
x = 0x0000000F     0000_0000_0000_0000_0000_0000_0000_1111
lsl w0, w1, #4     0000_0000_0000_0000_0000_0000_1111_0000   = x << 4  = x * 16
lsr w0, w1, #2     0000_0000_0000_0000_0000_0000_0000_0011   = x >> 2  = x / 4
```

`lsr` versus `asr` is signedness again: `unsigned >> n` fills with zeros,
`int >> n` replicates the sign bit so that `-8 >> 1 == -4`:

```
neg = -16          1111_1111_1111_1111_1111_1111_1111_0000
lsr w0, w1, #2     0011_1111_1111_1111_1111_1111_1111_1100   = 0x3ffffffc (nonsense)
asr w0, w1, #2     1111_1111_1111_1111_1111_1111_1111_1100   = -4        (correct)
```

### Two reasons a shift can *look* like it went the wrong way

**1. There is no `rol` instruction.** Rotate left by `n` is identical to rotate
right by `32 - n`, so AArch64 provides only `ror` and the compiler converts. Write
a left rotate and you get a right-rotate instruction with a different number:

```asm
; unsigned rotate_left_8(unsigned a) { return (a << 8) | (a >> 24); }
    ror w0, w0, #24            ; a LEFT rotate by 8, as a RIGHT rotate by 24

; unsigned rotate_left(unsigned a, int n) { return (a << n) | (a >> (32-n)); }
    neg w8, w1                 ; negate the amount (mod 32)...
    ror w0, w0, w8             ; ...and rotate right by it
```

This is the only case in the instruction set where the direction in your source
and the direction in the assembly genuinely disagree. `make run`-able proof is in
`integer.c`'s `rotate_left`, `rotate_left_8` and `rotate_right`.

**2. `lsl` as an operand modifier is not shifting the destination.** In
`ldr w0, [x0, x1, lsl #2]` or `movk x0, #0x1122, lsl #48`, the `lsl` scales *the
other operand before it is used* — it multiplies an index by 4, or moves an
immediate up into the high bits. Nothing is shifted in place, and no register is
modified by the shift itself. Read it as "×2ⁿ", not as an instruction.

A third source of confusion is purely a drawing convention: in the diagrams here
and in [docs/register-views.md](../docs/register-views.md), **bit 63 is on the
left and bit 0 on the right**, so a left shift moves data leftward in the
picture. Network- and RFC-style packet diagrams number bit 0 first and so put it
on the *left*, and in that layout a left shift appears to move data to the right.
The instruction never changed — only which end of the page bit 0 sits on. "Left"
always means *toward the most significant bit*.

**Bitfields in one instruction each:**

| Instruction | Does | Provoked by |
|---|---|---|
| `ubfx w0, w1, #5, #11` | extract 11 bits starting at bit 5, zero-extend | `extract_unsigned` |
| `sbfx` | the same, sign-extended | `extract_signed` |
| `bfi w0, w1, #8, #8` | **insert** 8 bits of `w1` at bit 8 of `w0` | `insert_field` |

These are why reading a bitfield (chapter 04) is cheap. Note that *writing* one
still needs a read-modify-write of the containing word, which is the concurrency
hazard mentioned there.

**Bit counting — each of these would be a loop if you wrote it by hand:**

| Instruction | Does | Provoked by |
|---|---|---|
| `clz` | count leading zeros | `count_leading_zeros` |
| `rbit` | reverse the bits | `reverse_bits` |
| `rbit` + `clz` | count *trailing* zeros: reverse, then count leading | `count_trailing_zeros` |
| `cnt.8b` + `addv.8b` | population count: count per byte, then sum the bytes | `population_count` |
| `rev` | reverse bytes — byte-order swap | `reverse_bytes` |

`population_count` is the one worth staring at: there is no scalar popcount on
arm64, so the value is moved into a *vector* register, counted per byte with
`cnt.8b`, and the eight byte-counts are summed with `addv.8b`. Four instructions,
and chapter 10 shows the compiler will only find them if you write the loop in
the shape it recognises.

## 8. Extension and narrowing

| Instruction | Does | Provoked by |
|---|---|---|
| `sxtw x0, w0` | sign-extend 32→64 | `widen_signed` |
| `sxtb` / `sxth` | sign-extend 8→32 / 16→32 | `widen_byte_signed` |
| `uxtb` / `uxth` | zero-extend | rarely emitted — see below |
| *(nothing)* | narrowing 64→32 | `narrow` |

Two asymmetries worth internalising:

- **Narrowing is free.** `(int)someLong` emits no instruction at all, because
  `w0` already *is* the low half of `x0`.
- **Zero-extension usually vanishes too**, because it gets folded into whatever
  produced the value: `ldrb` already zero-extends, and `and w0, w0, #0xff` can be
  merged into a neighbouring operation. You will hunt for a standalone `uxtb`
  in `integer.asm` and not find one. Sign-extension from memory is likewise
  folded — hence `ldrsb`, `ldrsh`, `ldrsw` as *load* instructions.

## 9. Loads and stores

arm64 is a load/store architecture: **arithmetic never touches memory.** Every
value must be loaded into a register, operated on, and stored back. That is why
`-O0` code (chapter 05) is a river of `ldr`/`str`.

**Addressing modes — each is one instruction:**

| Form | Computes | Provoked by |
|---|---|---|
| `ldr w0, [x0]` | `*p` | `load_plain` |
| `ldr w0, [x0, #12]` | `p[3]` — constant offset, **scaled** by the size | `load_offset` |
| `ldr w0, [x0, x1, lsl #2]` | `p[i]` — base + index×4 | `load_indexed` |
| `ldr w0, [x0], #4` | load, **then** `x0 += 4` (post-index) | `sum_walk` |
| `ldr w0, [x0, #4]!` | `x0 += 4`, **then** load (pre-index) | prologues |
| `ldur w0, [x0, #1]` | **unscaled**, possibly unaligned offset | `load_unaligned` |
| `ldp x0, x1, [sp, #16]` | load a **pair**: 16 bytes, one instruction | `load_pair` |

The scaling is why `#12` means "element 3" for an `int` array: the immediate is
multiplied by the access size. When an offset is not a multiple of the size, the
assembler must use the unscaled `ldur` form instead — that is the entire
difference between `ldr` and `ldur`.

**Size and signedness belong to the load, not the register:**

| Instruction | Loads | Provoked by |
|---|---|---|
| `ldrb` / `ldrh` | 1 / 2 bytes, zero-extended | `load_byte_unsigned` |
| `ldrsb` / `ldrsh` / `ldrsw` | 1 / 2 / 4 bytes, **sign**-extended | `load_byte_signed`, `load_word_signed` |
| `ldr w` / `ldr x` | 4 / 8 bytes | `load_plain` |
| `str`, `strb`, `strh`, `stp` | the store counterparts | `store_offset` |

`stp`/`ldp` are how every prologue and epilogue works, and how a 16-byte struct
is copied in one instruction:

```asm
    stp x29, x30, [sp, #-32]!    ; push a pair AND pre-decrement sp: one insn
    ldp x29, x30, [sp], #32      ; pop a pair AND post-increment sp
```

## 10. Conditional execution without branching

These consume the flags and produce a *value*, with no branch. They are why
chapter 01's `if/else` disappeared.

| Instruction | Does | Provoked by |
|---|---|---|
| `csel w0, w1, w2, gt` | `w0 = gt ? w1 : w2` | `select_max` |
| `cset w0, eq` | `w0 = eq ? 1 : 0` — materialise a bool | `set_if_equal` |
| `cinc` / `csinc` | `cond ? w1 : w2+1` | `select_inc` |
| `cneg` / `csneg` | `cond ? w1 : -w2` | `select_neg` |
| `csinv` | `cond ? w1 : ~w2` | chapter 02's `arith` |
| `ccmp w1, #2, #0, eq` | compare *only if* the first compare did not decide | `both_conditions` |

`ccmp` is the elegant one: **short-circuit `&&` without a branch.** It performs
the second comparison only when the first condition held, and otherwise injects a
fixed flag value. `both_conditions` (`a == 1 && b == 2`) becomes three
instructions and zero jumps.

## 11. Branches and calls

| Instruction | Does | Provoked by |
|---|---|---|
| `b label` | unconditional jump | every loop |
| `b.eq` / `b.lt` / `b.hi` … | jump if the flags match the condition | `branch_if_mask` |
| `cbz` / `cbnz w0, label` | compare with zero **and** branch — one instruction | `branch_if_zero` |
| `tbz` / `tbnz w0, #20, label` | branch on a **single bit** — one instruction | `branch_if_bit_set` |
| `bl f` | **branch with link**: call. Return address → `lr`/`x30` | `call_direct` |
| `blr x8` | call the address *in a register* | `call_indirect` |
| `br x8` | jump to an address in a register | jump tables, tail calls |
| `ret` | jump to `lr` | every function |

Four things this table tells you:

- **A call does not touch the stack.** `bl` puts the return address in a
  *register*. A leaf function therefore needs no stack traffic at all (chapter
  03), which x86-64 cannot match because `call` pushes.
- **`cbz` and `tbnz` fuse a compare into the branch**, so testing a pointer for
  null, or one bit of a flags word, costs a single instruction.
- **`br` versus `bl`/`blr` is the tail-call distinction.** `call_tail` compiles to
  `b _callee` — no return address saved, no frame, the callee's `ret` returns
  directly to *our* caller. This is chapter 03's tail-call elimination, and
  chapter 06's virtual call ends in `br` for the same reason.
- **`blr`/`br` through a register is the universal runtime indirection** — virtual
  calls, jump tables, function pointers, closures, and coroutine resumption are
  all this one instruction.

## 12. Floating point

| Instruction | Does | Provoked by |
|---|---|---|
| `fadd` / `fsub` / `fmul` / `fdiv` | the four operations | `f_add`, `d_mul`, `f_div` |
| `fneg` / `fabs` | negate / absolute value | `f_neg`, `f_abs` |
| `fsqrt` | square root — **one instruction** | `f_sqrt` |
| `fmadd` | fused multiply-add: `a*b + c`, **one rounding** | `f_fma` |
| `fcmp` | compare, setting the same NZCV flags | `f_compare` |
| `fcsel` | select, like `csel` but for float registers | `f_select` |
| `scvtf` | signed integer → float | `int_to_float` |
| `fcvtzs` | float → signed integer, rounding **toward zero** | `float_to_int` |
| `fcvt` | float ↔ double | `float_to_double` |
| `fmov` | move bits between register files, no conversion | `float_bits` |

Three details that cause real bugs:

- **`fmadd` rounds once**, a separate `fmul` + `fadd` rounds twice, so they give
  different answers. The compiler is allowed to fuse them (C's
  `FP_CONTRACT`), which is why identical source can differ across compilers.
- **`fcvtzs` truncates**, it does not round — that is the `z` ("toward zero").
  `(int)-1.5` is `-1`, matching C's rule, and the instruction was chosen to match.
- **`fmov w0, s0` is not a conversion.** It reinterprets the bits, which is what
  chapter 04's union type-pun compiles to. `scvtf`/`fcvtzs` change the value;
  `fmov` changes only which register file holds it.

## 13. SIMD

Vector instructions carry a lane suffix: `add.4s` is four 32-bit adds in one
instruction. Full vectorisation only appears at `-O2` and above:

```sh
make OPT=O2 mnemonics
```

| Instruction | Does | Provoked by |
|---|---|---|
| `add.4s` | four 32-bit adds at once | `v_add`, `v_explicit_add` |
| `fmla.4s` | four fused multiply-adds | `v_explicit_fma` |
| `movi.2d v0, #0` | set a whole vector register to zero | loop accumulator init |
| `ldp q0, q1, [x8]` | load **32 bytes** in one instruction | `v_add` at `-O2` |
| `addv.4s` | **horizontal** add: sum the lanes into a scalar | `v_sum` at `-O2` |
| `mov.16b` / `mov.s w0, v0[2]` | whole-vector copy / extract one lane | `v_lane` |

The pattern to recognise: a vectorised reduction uses several `movi.2d`
accumulators, `add.4s` inside the loop, and one `addv` at the end to collapse the
lanes. Chapter 01 exercise 2 has you count them.

`v_fsum` is **not** vectorised at any optimisation level, while `v_sum` is. Float
addition is not associative, so reassociating the loop would change the result,
and the compiler will not do that without `EXTRA=-ffast-math`. This is the
clearest example in the repo of an optimisation blocked by semantics rather than
by difficulty.

## 14. Atomics and barriers

Covered in full in [chapter 11](../11-atomics-and-memory-order/note.md); the
short version:

| Instruction | Does |
|---|---|
| `ldadd` / `ldaddal` | atomic add, relaxed / with acquire+release |
| `swp` / `swpa` | atomic exchange |
| `cas` / `casal` | compare-and-swap |
| `ldxr` / `stxr` | load-exclusive / store-conditional (the pre-8.1 retry loop) |
| `ldar` / `stlr` | load-**a**cquire / store-re**l**ease |
| `ldapr` | load-acquire, processor-consistent |
| `dmb ish` | a standalone memory barrier |

## 15. The odd ones

| Instruction | Does | Provoked by |
|---|---|---|
| `brk #1` | deliberate trap — how `__builtin_trap`, `assert`, and UB sanitisers stop | `crash` |
| `nop` | do nothing (alignment padding) | `.p2align` |
| `svc #0` | supervisor call: **the actual syscall instruction** | not reachable from C |
| `isb` / `dsb` | instruction / data synchronisation barrier | kernel code |

`svc` is the boundary of this whole repo: everything a program cannot do for
itself — create a thread, open a file, map memory, sleep — is a `svc` into the
kernel. You will not find one in these labs, because libc makes the call on your
behalf, which is exactly chapter 12's point.

## 16. Reading an instruction you have never seen

The names are systematic. Decode the suffixes and you can usually guess:

| Affix | Means | Example |
|---|---|---|
| `w` / `x` operand | 32-bit / 64-bit | `add w0,…` vs `add x0,…` |
| trailing `s` | **sets the flags** | `adds`, `subs`, `ands` |
| leading `c` | **conditional** | `csel`, `cset`, `cinc`, `ccmp` |
| leading `f` | **floating point** | `fadd`, `fcmp`, `fcsel` |
| leading `u` / `s` | **unsigned / signed** | `udiv`/`sdiv`, `uxtb`/`sxtb` |
| trailing `b` `h` `w` on a load | byte / halfword / word | `ldrb`, `ldrsh` |
| `p` in `ldp`/`stp` | **pair** of registers | `stp x29, x30, …` |
| `u` in `ldur`/`stur` | **unscaled** offset | `ldur w0, [x0, #1]` |
| `.4s` `.2d` `.8b` `.16b` | vector lane arrangement | `add.4s` |
| `a` / `l` | **a**cquire / re**l**ease ordering | `ldar`, `stlr`, `ldaddal` |
| `x` in `ldxr`/`stxr` | e**x**clusive (LL/SC) | `ldxr w0, [x8]` |
| `v` | horizontal, **across** lanes | `addv.4s` |

So `ldaddal` decodes as load-add, **a**cquire and re**l**ease — and `sbfx` as
signed bitfield extract, without ever opening the manual.

## Exercises

1. `make mnemonics` and pick any instruction you cannot explain. Find the
   function that produced it, and predict what changes if you alter the C.
2. Write a function that emits `csinv`. (Hint: chapter 02's `arith` has one —
   what shape produces "the inverse of the other operand"?)
3. Confirm the immediate boundaries above for yourself: find the largest constant
   that still fits in **one** `add`, then the largest that fits in two. Why does
   `x + 0xFFFFFF` use `mov`+`add` when two shifted adds would also reach it?
4. `population_count` detours through a vector register. Write a scalar-only
   popcount and count the instructions. Now measure which is faster — the answer
   is not the shorter one.
5. Provoke `ldxr`/`stxr` without writing atomics, using
   `EXTRA="-mcpu=apple-m1+nolse"` on chapter 11. Then explain why a compiler
   cannot use that pair to implement a mutex fast path on its own.
6. Compile `integer.c` for x86-64 (`EXTRA="--target=x86_64-linux-gnu"` if you have
   the headers) and rebuild the tables in sections 4–11. Which arm64 instructions
   have no single x86 equivalent, and which x86 instructions have no arm64 one?
