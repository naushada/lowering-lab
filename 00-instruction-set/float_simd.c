/* Floating point and SIMD live in a SEPARATE register file: v0-v31, addressed as
   s0-s31 (32-bit float), d0-d31 (64-bit double) or q0-q31 (128-bit vector).
   Arguments and returns use v0-v7, exactly parallel to x0-x7. */

#define N __attribute__((noinline))

/* ======================= 1. SCALAR FLOATING POINT ====================== */

N float  f_add(float a, float b)  { return a + b; }        /* fadd s0,s0,s1 */
N double d_mul(double a, double b){ return a * b; }        /* fmul d0,d0,d1 */
N float  f_div(float a, float b)  { return a / b; }        /* fdiv: still slow */
N float  f_neg(float a)           { return -a; }           /* fneg */
N float  f_abs(float a)           { return __builtin_fabsf(a); }  /* fabs */
N float  f_sqrt(float a)          { return __builtin_sqrtf(a); }  /* fsqrt: ONE insn */

/* Fused multiply-add: one instruction, and one rounding instead of two -- which
   is why it is not the same answer as a separate multiply and add. */
N float  f_fma(float a, float b, float c) { return a * b + c; }   /* fmadd */

/* Comparison sets the same NZCV flags the integer unit uses, so csel/cset work
   on float comparisons too. */
N int    f_compare(float a, float b) { return a < b; }            /* fcmp + cset */
N float  f_select(float a, float b)  { return a > b ? a : b; }    /* fcmp + fcsel */

/* Conversions are explicit instructions, and the direction matters:
   scvtf = signed convert to float, fcvtzs = float convert to signed, toward zero. */
N float  int_to_float(int x)      { return (float)x; }     /* scvtf */
N int    float_to_int(float x)    { return (int)x; }       /* fcvtzs: TRUNCATES */
N double float_to_double(float x) { return x; }            /* fcvt  */
N float  double_to_float(double x){ return (float)x; }     /* fcvt  */

/* Moving bits between the two register files, without converting the value.
   This is what a union-based type pun (chapter 04) actually compiles to. */
N unsigned float_bits(float f) { unsigned u; __builtin_memcpy(&u,&f,4); return u; } /* fmov w0,s0 */

/* ======================= 2. SIMD ====================================== */

/* One instruction, four lanes. `restrict` promises the arrays do not overlap,
   without which the compiler must emit a runtime overlap check (see chapter 10's
   copy_loop) or refuse to vectorise at all. */
N void v_add(int *restrict d, const int *restrict a, const int *restrict b, int n) {
    for (int i = 0; i < n; i++) d[i] = a[i] + b[i];        /* add.4s over ldp q */
}

/* A reduction: the loop-carried dependency has to be broken into several
   accumulators, which is legal for integers... */
N int v_sum(const int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];                 /* add.4s + addv */
    return s;
}

/* ...and NOT legal for floats, because floating-point addition is not
   associative. Build with EXTRA=-ffast-math to grant permission and watch this
   one change completely. */
N float v_fsum(const float *a, int n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}

/* Explicit vector types, when you do not want to hope. */
typedef int  int4   __attribute__((ext_vector_type(4)));
typedef float float4 __attribute__((ext_vector_type(4)));
N int4   v_explicit_add(int4 a, int4 b)   { return a + b; }        /* add.4s  */
N float4 v_explicit_fma(float4 a, float4 b, float4 c) { return a * b + c; } /* fmla.4s */
N int    v_lane(int4 a) { return a[2]; }                           /* mov w0,v0.s[2] */
