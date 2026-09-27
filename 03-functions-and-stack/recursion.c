/* Recursion needs no feature: each call gets a fresh slice of stack, so each
   gets fresh locals. The "call stack" is one register (sp) plus a discipline. */

long fact_rec(long n) {
    if (n <= 1) return 1;
    return n * fact_rec(n - 1);        /* NOT a tail call: the multiply happens
                                          after the call returns, so the frame
                                          must stay alive. Real recursion. */
}

/* Tail position: nothing left to do after the call. The compiler can reuse the
   frame and turn `bl; ret` into `b` -- a jump. Recursion becomes a loop, and
   the stack stops growing. Look for `b _fact_tail` or, more often, no call at
   all because it became a real loop. */
long fact_tail(long n, long acc) {
    if (n <= 1) return acc;
    return fact_tail(n - 1, n * acc);
}

long fact_iter(long n) {
    long acc = 1;
    while (n > 1) { acc *= n; n--; }
    return acc;
}

/* Mutual recursion, both in tail position. */
int is_even(unsigned n);
int is_odd(unsigned n)  { return n == 0 ? 0 : is_even(n - 1); }
int is_even(unsigned n) { return n == 0 ? 1 : is_odd(n - 1); }

/* Deep non-tail recursion is how you hit a stack overflow: 8 MB of stack
   divided by the frame size is your recursion limit, and the frame size is
   decided by the compiler, not by you. Read fib's prologue to see the size. */
long fib(long n) { return n < 2 ? n : fib(n-1) + fib(n-2); }
