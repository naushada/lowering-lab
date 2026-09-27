# 02 — Switch, and the four ways to dispatch

**Claim:** `switch` is not a construct, it is a *decision* the compiler makes.
The same keyword becomes arithmetic, a data table, a binary search tree, or an
indirect branch, depending on your case values and bodies.

```sh
make table      # the whole lesson in one screen
make            # then read out/O1/shapes.asm
```

```
FUNCTION       INSNS  BLOCKS        what the compiler chose
_arith             6       0        arithmetic: no branch at all
_lut               8       1        one bounds check + one load from a table
_tree             31       8        binary search tree of compares
_jumptable        23      13        one bounds check + one indirect branch
```

## 1. `arith` — the switch that isn't

The cases are 100, 111, 122, … step 11. So there is nothing to look up:

```asm
    mov  w8, #11
    mov  w9, #100
    madd w8, w0, w8, w9      ; w8 = k*11 + 100
    cmp  w0, #8
    csinv w0, w8, wzr, lo    ; out of range -> -1, branchlessly
```

Eight cases, zero branches. The `default` did not vanish — it *became* the range
check. Whenever your case values have structure, expect the structure to be
found and exploited.

## 2. `lut` — unrelated constants, but no code

No formula fits 41, 7, 1000, −3, … but every case only *returns a value*. So
the values go into `.const` and the switch becomes a bounds check and a load:

```asm
    adrp x8, l_switch.table.lut@PAGE      ; the table of .long values
    ldr  w0, [x8, x9, lsl #2]             ; answer = table[k]
```

`is_vowel` in `sparse.c` is the sharper version: it becomes a **21-byte table**
indexed by `c - 'a'`, read with one `ldrb`. Five `case` labels, one load, no
branch per case. Control flow turned into data.

## 3. `tree` — eight cases, each running different code

Now a table of values is useless, because each case must *call* something. Eight
cases is below this target's jump-table threshold, so you get a decision tree —
note it splits at 3, then 1, then 0, which is a binary search, **not** the chain
of `if`s you wrote it as. 8 blocks, ~3 compares to reach any case.

## 4. `jumptable` — twelve cases crosses the threshold

```asm
    cmp  w0, #11                          ; one range check
    adrp x9, LJTI3_0@PAGE                 ; the table
    adr  x10, LBB3_2                      ; the base label
    ldrb w11, [x9, x8]                    ; one BYTE: (target - base) >> 2
    add  x10, x10, x11, lsl #2            ; base + offset*4
    br   x10                              ; indirect branch
```

Constant time no matter how many cases you add. Two details worth keeping:

- The table holds **byte** offsets, `(LBB3_9 - LBB3_2) >> 2`, not 8-byte
  addresses. Instructions are 4-byte aligned so the low 2 bits are always zero,
  and the whole table for twelve cases is twelve bytes.
- `br x10` is an *indirect* branch. The predictor must now guess a target rather
  than a direction, which is a different and worse prediction problem. This is
  the same machinery as a virtual call (chapter 06) — and the same cost.

The threshold is a tuning knob, not a law:

```sh
make OPT=O1 EXTRA="-mllvm -min-jump-table-entries=4" asm   # force tables earlier
make OPT=O1 EXTRA="-mllvm -min-jump-table-entries=99" asm  # forbid them
```

## 5. Sparse values: a tree, never a table

`sparse()` switches on 1, 1000, 50000, 999999, −7. A jump table would need a
million entries, so the compiler bisects: `cmp w0, #999` first, splitting the
set in half, then compares within each half. Five cases, three compares worst
case.

`is_alnum` shows the range idiom: `'0'..'9'` becomes *subtract the low bound,
then one unsigned compare against the width* — `sub w8, w0, #48; cmp w8, #9;
b.hi`. Two range tests become two compares, not four, because an unsigned
compare catches "below the bound" as a huge positive number for free.

## 6. Strings: why no language can switch on them directly

There is no instruction that compares bytes at two addresses. `make table`:

```
_by_strcmp    27 insns   3 calls      call strcmp until one matches
_by_shape     41 insns   1 call       switch on length, then compare bytes inline
_by_packing   23 insns   0 calls      pack 4 bytes into an int, switch on that
```

`by_packing` is the trick behind fast keyword lookup: convert the string into an
integer and you are back to case 1–4 above, jump table included. Java and C#
`switch` on strings do the same thing with a hash instead of a raw pack, then
confirm with an equality check because hashes collide.

## Exercises

1. Add a 9th, 10th, 11th case to `tree` one at a time. At which count does
   `make table` switch from many BLOCKS to an indirect `br`? Does the answer
   change with `-Os`?
2. Change `arith`'s step from 11 to 12 and then to a non-pattern. Watch `madd`
   become a table.
3. Make one case in `lut` return `f0()` instead of a constant. The whole table
   strategy collapses — why must it?
4. Give `jumptable` a hole (delete `case 5`). What does the table entry for 5
   point at now?
