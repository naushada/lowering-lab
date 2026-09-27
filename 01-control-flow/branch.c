/* if/else lowers to compare + conditional branch -- until the compiler decides
   the branch is more expensive than just computing both sides. Then it emits
   csel (arm64) / cmov (x86-64) and the control flow disappears into data flow.
   This is "if-conversion", and it is why branchless code is often not something
   you write, but something you are given. */

int max_if(int a, int b) {
    if (a > b) return a;
    else       return b;
}

int max_ternary(int a, int b) {
    return a > b ? a : b;      /* identical assembly to max_if */
}

int clamp(int x, int lo, int hi) {
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return x;
}

/* Three-way: watch how many compares survive, and in what order. */
int sign(int x) {
    if (x > 0) return  1;
    if (x < 0) return -1;
    return 0;
}

/* Nesting is not a thing in the output -- it is flattened into a chain. */
int nested(int a, int b, int c) {
    if (a) { if (b) { if (c) return 3; return 2; } return 1; }
    return 0;
}
