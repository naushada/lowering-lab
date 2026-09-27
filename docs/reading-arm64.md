# Reading arm64 assembly (enough to do these labs)

This machine is Apple silicon, so `clang -S` gives you AArch64. You do not need
the 1,200-page manual; you need about thirty instructions.

## Registers

| Name | Meaning |
|---|---|
| `x0`–`x7` | first eight integer/pointer arguments, and `x0` is the return value |
| `w0`–`w7` | the low 32 bits of the same registers. `w` = 32-bit, `x` = 64-bit. An `int` lives in `w`, a pointer in `x` |
| `x8` | indirect result location (a struct returned by value); also a scratch register |
| `x9`–`x15` | caller-saved scratch: a called function may destroy these |
| `x19`–`x28` | callee-saved: if a function uses them it must save and restore them. **Seeing these saved in a prologue means a value had to survive a call** |
| `x29` / `fp` | frame pointer |
| `x30` / `lr` | link register: `bl` puts the return address here |
| `sp` | stack pointer, must stay 16-byte aligned |
| `xzr`/`wzr` | the zero register: reads as 0, writes are discarded |

## The instructions you will actually see

```
mov  w8, w1          register copy (or load a constant: mov w8, #42)
ldr  w10, [x0]       load 4 bytes from the address in x0
ldr  w10, [x0, #8]   load from x0+8            <- struct field
ldr  w10, [x0, x1, lsl #2]  load from x0 + x1*4  <- array index
ldr  w10, [x0], #4   load from x0, THEN x0 += 4   <- post-increment, walks an array
stp  x29, x30, [sp, #-32]!  push a PAIR and pre-decrement sp  <- prologue
ldp  x29, x30, [sp], #32    pop a pair and post-increment sp  <- epilogue
add / sub / mul / and / orr / eor / lsl / lsr / asr    arithmetic
subs x9, x9, #1      subtract AND set the condition flags (the 's' suffix)
cmp  w0, w1          compare = subs to nowhere; only sets flags
b    LBB0_2          unconditional jump
b.lt / b.ne / b.eq   jump if the flags say so
cbz  w0, LBB0_2      compare-with-zero and branch, fused into one instruction
csel w0, w0, w1, gt  w0 = (gt ? w0 : w1)   <- a branch turned into data flow
cset w0, ne          w0 = (ne ? 1 : 0)     <- a bool materialised
bl   _f              call: jump and set lr
blr  x8              call the address IN x8  <- every virtual call, every callback
ret                  jump to lr
```

## x86-64, if that is what you know

| arm64 | x86-64 |
|---|---|
| `x0..x7` args | `rdi, rsi, rdx, rcx, r8, r9` args (SysV) |
| `bl f` / `ret` | `call f` / `ret` (return address goes on the stack, not a register) |
| `csel` | `cmov` |
| `ldr w0,[x1,x2,lsl #2]` | `mov eax,[rsi+rdx*4]` |
| `stp/ldp` prologue | `push rbp; mov rbp,rsp` |
| `ldadd` | `lock xadd` |
| `dmb ish` | `mfence` |

The important difference for reading: x86-64 is two-operand and destructive
(`add eax, ebx` means `eax += ebx`), arm64 is three-operand (`add w0, w1, w2`
means `w0 = w1 + w2`). arm64 is also fixed-width and load/store only — it
cannot do arithmetic straight on memory, so you see explicit `ldr`/`str`.

## Local labels

`LBB0_2` is "**L**ocal **B**asic **B**lock, function 0, block 2". Counting the
`LBB` labels in a function counts its basic blocks, which is a decent proxy for
how much control flow survived.

---

This page is deliberately short. For the full reference — every instruction with
the C that provokes it, the NZCV flags and all sixteen condition codes, every
addressing mode, constant-encoding limits, and a suffix decoder — see
[chapter 00](../00-instruction-set/note.md), which regenerates its own examples:

```sh
cd 00-instruction-set && make mnemonics
```
