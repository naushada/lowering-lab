/* What a stack frame actually is: sp moves down once, and every local is a
   fixed offset from it. There are no "variables" -- only offsets. */

void sink(void *);

void small_frame(void) {
    int a = 1, b = 2;
    int arr[2] = { a, b };
    sink(arr);                  /* taking the address forces it into memory */
}

void big_frame(void) {
    char buf[4096];             /* watch the single `sub sp, sp, #...` */
    buf[0] = 7;
    sink(buf);
}

/* Variable-length array: the frame size is not a constant, so sp is adjusted by
   a computed amount and must be restored from the frame pointer afterwards.
   This is why x29 exists. */
void vla(int n) {
    char buf[n];
    buf[0] = 1;
    sink(buf);
}

/* Two scopes, no overlap in lifetime: the SAME stack slot is reused for both
   arrays. Scope is a compile-time fiction about names; the storage is shared. */
void reuse(void) {
    { int p[8]; p[0] = 1; sink(p); }
    { int q[8]; q[0] = 2; sink(q); }
}
