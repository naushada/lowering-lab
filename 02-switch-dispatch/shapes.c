/* A `switch` is not one construct. The compiler picks from four completely
   different lowerings depending on the case values and bodies, and the choice
   changes the cost from O(log n) branches to one indirect jump to no branch at
   all. `make table` shows the difference in one screen. */

int f0(void), f1(void), f2(void),  f3(void),  f4(void),  f5(void);
int f6(void), f7(void), f8(void),  f9(void),  f10(void), f11(void);

/* --- 1. ARITHMETIC. The case values form a progression (100, 111, 122, ...
   step 11), so there is nothing to dispatch on: the answer is 100 + 11*k.
   Expect `madd` and a `csinv` doing the range check. Zero branches. --- */
int arith(int k) {
    switch (k) {
        case 0: return 100;
        case 1: return 111;
        case 2: return 122;
        case 3: return 133;
        case 4: return 144;
        case 5: return 155;
        case 6: return 166;
        case 7: return 177;
    }
    return -1;
}

/* --- 2. DATA TABLE. Unrelated constants, so no formula -- but every case
   RETURNS A VALUE rather than running code. The values go in a table in .const
   and the switch becomes one bounds check plus one load. Still zero branches
   on the hot path; look for `l_switch.table.lut` and `.long`. --- */
int lut(int k) {
    switch (k) {
        case 0: return 41;
        case 1: return 7;
        case 2: return 1000;
        case 3: return -3;
        case 4: return 12;
        case 5: return 88;
        case 6: return 5;
        case 7: return 63;
    }
    return -1;
}

/* --- 3. BINARY SEARCH TREE. Now each case runs different CODE, so a table of
   values will not do. Eight cases is below this target's jump-table threshold,
   so the compiler emits a decision tree: ~log2(8) = 3 compares deep, not 8.
   Note it splits at 3, then 1, then 0 -- a tree, not a chain. --- */
int tree(int k) {
    switch (k) {
        case 0: return f0();
        case 1: return f1();
        case 2: return f2();
        case 3: return f3();
        case 4: return f4();
        case 5: return f5();
        case 6: return f6();
        case 7: return f7();
    }
    return -1;
}

/* --- 4. JUMP TABLE. The same thing with twelve cases crosses the threshold.
   Now: one range check, one load from a table of addresses, one INDIRECT branch
   (`br x8`). Constant time regardless of how many cases you add -- but the
   branch predictor now has to guess a target instead of a direction. This is
   the same machinery as a virtual call (chapter 06). --- */
int jumptable(int k) {
    switch (k) {
        case 0:  return f0();
        case 1:  return f1();
        case 2:  return f2();
        case 3:  return f3();
        case 4:  return f4();
        case 5:  return f5();
        case 6:  return f6();
        case 7:  return f7();
        case 8:  return f8();
        case 9:  return f9();
        case 10: return f10();
        case 11: return f11();
    }
    return -1;
}

/* Fallthrough is not a feature: it is the ABSENCE of a jump at the end of a
   case body. Which is exactly why forgetting `break` is such a good bug. */
int fallthrough(int k) {
    int r = 0;
    switch (k) {
        case 3: r += 3;   /* fall through */
        case 2: r += 2;   /* fall through */
        case 1: r += 1;
    }
    return r;
}
