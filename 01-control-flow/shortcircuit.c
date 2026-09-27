/* && and || are control flow, not arithmetic. There is no "and" instruction
   involved in `x && y` -- there is a second basic block that may be skipped.
   Compare and_shortcircuit against and_bitwise: one calls f twice sometimes,
   the other calls f twice always. */

int f(int);

int and_shortcircuit(int a, int b) { return f(a) && f(b); }
int and_bitwise(int a, int b)      { return f(a) &  f(b); }
int or_shortcircuit(int a, int b)  { return f(a) || f(b); }

/* The idiom this exists for: the guard must be evaluated first, or the
   dereference happens on a null pointer. The ordering is a hard guarantee of
   the language, so no optimiser may swap these two operands. */
int guarded(const int *p, int x) { return p && *p == x; }

/* Cheap operands, no side effects: now the compiler is free to flatten both
   branches into arithmetic, and usually does. */
int both_cheap(int a, int b) { return (a > 0) && (b > 0); }
