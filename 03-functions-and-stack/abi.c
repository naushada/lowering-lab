/* The calling convention is not enforced by hardware. It is a treaty:
   args in x0-x7, return in x0, x19-x28 preserved, sp 16-byte aligned.
   Break it and nothing traps -- you just get garbage. */

int few(int a, int b, int c) { return a + b + c; }

/* Nine arguments. Eight fit in registers; the ninth goes on the stack, and the
   CALLER puts it there. Look at what call_many does before `bl`, and at the
   `ldr w8, [sp]` inside many() that reads it back. */
int many(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    return a + b + c + d + e + f + g + h + i;
}
int call_many(void) { return many(1,2,3,4,5,6,7,8,9); }

/* A leaf function -- calls nothing -- needs NO prologue at all: nothing must be
   preserved because nothing can clobber it. Compare with not_leaf below. */
int leaf(int x) { return x * 3 + 1; }

int other(int);
int not_leaf(int x) {
    int y = x * 3;            /* y must survive the call... */
    return y + other(x);      /* ...so it needs a callee-saved register, so the
                                 prologue appears. The cost of calling is real. */
}

/* Returning a struct too big for registers: the caller passes a hidden pointer
   to space it already allocated, in x8. "Return by value" is a lie told by the
   language; the copy happens at the destination. */
struct big { long a, b, c, d, e; };
struct big make_big(long v) {
    struct big b = { v, v+1, v+2, v+3, v+4 };
    return b;
}
