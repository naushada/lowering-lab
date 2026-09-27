/* Indexing is arithmetic: &a[i] == a + i*sizeof(*a). The scale is folded into
   the addressing mode, so it costs nothing -- until the element size is not a
   power of two, and then you see a real multiply. */

int   idx_int (const int *a, long i)   { return a[i]; }      /* lsl #2 */
short idx_short(const short *a, long i){ return a[i]; }      /* lsl #1 */
long  idx_long(const long *a, long i)  { return a[i]; }      /* lsl #3 */

struct three { int a, b, c; };                               /* 12 bytes: not a
                                                                power of two */
int idx_struct(const struct three *a, long i) { return a[i].b; }

/* 2D arrays are 1D with a multiply. Row-major means the LAST index is
   contiguous, which is the entire reason loop order changes performance. */
int m_rowmajor(const int m[8][16], long r, long c) { return m[r][c]; }   /* r*16 + c */

/* An array parameter is a pointer. sizeof tells the truth; the signature lies. */
long size_of_param(int a[10]) { return sizeof a; }           /* 8, not 40 */

/* Pointer arithmetic on a struct pointer, the manual version of the above. */
int manual(const struct three *base, long i) {
    const char *p = (const char *)base;
    return *(const int *)(p + i * sizeof(struct three) + __builtin_offsetof(struct three, b));
}
