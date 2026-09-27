/* Every loop in every language is the same three pieces:
     a test, a conditional jump out, an unconditional jump back.
   These four functions are written differently on purpose.
   Compare their assembly: some of them are character-for-character identical. */

int sum_while(const int *a, int n) {
    int s = 0, i = 0;
    while (i < n) { s += a[i]; i++; }
    return s;
}

int sum_for(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

/* do/while tests at the bottom: one fewer jump, and it always runs once.
   This is the shape optimisers rotate every loop into ("loop rotation"),
   which is why the -O2 output of sum_while looks like this one. */
int sum_do(const int *a, int n) {
    int s = 0, i = 0;
    do { s += a[i]; i++; } while (i < n);
    return s;
}

/* break and continue are not constructs. They are jumps to two labels the
   loop already had: continue -> the latch, break -> the exit. */
int first_negative(const int *a, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] == 0) continue;
        if (a[i] < 0)  break;
    }
    return -1;
}
