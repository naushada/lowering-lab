/* An instruction reference you can regenerate.
   Every function below exists to provoke ONE instruction or addressing mode, so
   you can read the C and the assembly side by side:

       make            then open out/O1/integer.asm
       make mnemonics  the full inventory of what got emitted

   Nothing here is interesting as a program. The point is the instruction. */

#include <stdint.h>

#define N __attribute__((noinline))       /* keep each one separate */

/* ======================= 1. MOVING AND MAKING CONSTANTS ================= */

/* Instructions are fixed 32 bits wide, so a 64-bit constant cannot fit inside
   one. It is built up 16 bits at a time: movz (move-zeroing) then movk
   (move-keeping). This is why large constants cost several instructions. */
N long   const_small(void)  { return 42; }                    /* mov  */
N long   const_16bit(void)  { return 0x1234; }                /* mov  */
N long   const_32bit(void)  { return 0x12345678; }            /* mov + movk */
N long   const_64bit(void)  { return 0x123456789ABCDEF0L; }   /* mov + 3x movk */
N long   const_negative(void){ return -1; }                   /* mov #-1: one insn */
N int    move_reg(int a, int b) { (void)a; return b; }        /* mov w0, w1 */
N int    bitwise_not(int x) { return ~x; }                    /* mvn */
N int    negate(int x)      { return -x; }                    /* neg = sub from zr */

/* ======================= 2. ARITHMETIC AND FLAGS ======================== */

/* add/sub take a 12-bit immediate (optionally shifted by 12). Bigger constants
   must be materialised into a register first -- compare these two. */
N int    add_small_imm(int x) { return x + 100; }          /* add w0, w0, #100 */
N int    add_large_imm(int x) { return x + 0x123456; }     /* mov+movk, then add */

/* Every arithmetic instruction can shift its second operand for free, in the
   same cycle. This is why `x + y*8` costs one instruction, not two. */
N int    add_shifted(int x, int y) { return x + (y << 3); } /* add w0,w0,w1,lsl #3 */
N long   add_extended(long x, int y) { return x + y; }      /* add x0,x0,w1,sxtw  */

/* The 's' suffix means "and set the condition flags" (NZCV).
   cmp is literally subs into the zero register; cmn is adds into it. */
N int    compare(int a, int b) { return a == b; }           /* cmp + cset */

/* Multi-word arithmetic is why carry exists: adds produces a carry, adc
   consumes it. __int128 forces the pair to be visible. */
N unsigned __int128 add128(unsigned __int128 a, unsigned __int128 b) { return a + b; }

/* ======================= 3. MULTIPLY AND DIVIDE ========================= */

/* Multiply-accumulate is a single instruction, so `a*b + c` is never two. */
N int    multiply(int a, int b)            { return a * b; }        /* mul   */
N int    mul_add(int a, int b, int c)      { return a * b + c; }    /* madd  */
N int    mul_sub(int a, int b, int c)      { return c - a * b; }    /* msub  */
N long   widening_signed(int a, int b)     { return (long)a * b; }  /* smull */
N uint64_t widening_unsigned(uint32_t a, uint32_t b) { return (uint64_t)a * b; } /* umull */

/* The high half of a 64x64 multiply -- the instruction behind every
   "divide by a constant" and every 128-bit hash. */
N uint64_t multiply_high(uint64_t a, uint64_t b) {
    return (uint64_t)(((unsigned __int128)a * b) >> 64);                /* umulh */
}

/* There is no remainder instruction. `a % b` is a divide, then a
   multiply-subtract to recover the remainder: two instructions, not one. */
N int      divide_signed(int a, int b)     { return a / b; }   /* sdiv */
N unsigned divide_unsigned(unsigned a, unsigned b) { return a / b; } /* udiv */
N int      modulo(int a, int b)            { return a % b; }   /* sdiv + msub */

/* ======================= 4. LOGIC, SHIFTS, BITS ========================= */

N int      bit_and(int a, int b)  { return a & b; }     /* and */
N int      bit_or(int a, int b)   { return a | b; }     /* orr */
N int      bit_xor(int a, int b)  { return a ^ b; }     /* eor */
N int      bit_clear(int a, int b){ return a & ~b; }    /* bic: and-with-complement */
N int      bit_test(int a)        { return (a & 8) != 0; } /* tst (or ubfx/and) */

N unsigned shift_left(unsigned a, int n)  { return a << n; }  /* lsl */
N unsigned shift_right_logical(unsigned a, int n) { return a >> n; } /* lsr */
N int      shift_right_arith(int a, int n){ return a >> n; }  /* asr: keeps the sign */
N unsigned rotate_right(unsigned a, int n) { return (a >> n) | (a << (32 - n)); } /* ror */

/* THERE IS NO `rol` INSTRUCTION. A rotate left by n is a rotate right by 32-n,
   so writing a LEFT rotate produces a RIGHT-rotate instruction with a different
   immediate. This is the one place where the direction in the source and the
   direction in the assembly genuinely disagree -- see the note. */
N unsigned rotate_left(unsigned a, int n) { return (a << n) | (a >> (32 - n)); }
                                        /* neg + ror: negate the amount, rotate right */
N unsigned rotate_left_8(unsigned a) { return (a << 8) | (a >> 24); }  /* ror #24 (!) */

/* Bitfield extraction and insertion are single instructions, which is what
   makes chapter 04's bitfields cheap to READ. */
N unsigned extract_unsigned(unsigned x) { return (x >> 5) & 0x7FF; }   /* ubfx */
N int      extract_signed(int x)        { return (x << 11) >> 21; }    /* sbfx */
N unsigned insert_field(unsigned dst, unsigned src) {
    return (dst & ~(0xFFu << 8)) | ((src & 0xFF) << 8);                /* bfi  */
}

/* Bit-counting instructions. Each of these is one instruction on arm64, and each
   would be a loop if you wrote it out (chapter 10). */
N int      count_leading_zeros(unsigned x) { return __builtin_clz(x); }   /* clz */
N int      count_trailing_zeros(unsigned x){ return __builtin_ctz(x); }   /* rbit + clz */
N int      population_count(unsigned x)    { return __builtin_popcount(x);}/* cnt */
N unsigned reverse_bytes(unsigned x)       { return __builtin_bswap32(x); }/* rev */
N unsigned reverse_bits(unsigned x)        { return __builtin_bitreverse32(x); } /* rbit */

/* Sign and zero extension: narrowing a type is free, WIDENING is an
   instruction, and which one depends on signedness. */
N long     widen_signed(int x)           { return x; }    /* sxtw */
N unsigned long widen_unsigned(unsigned x){ return x; }   /* mov w -> implicit zero */
N int      widen_byte_signed(signed char c) { return c; } /* sxtb */
/* A separate uxtb/uxth almost never appears: the extension is folded into the
   load (ldrb already zero-extends) or into the arithmetic. Narrowing is free. */
N int      widen_byte_unsigned(unsigned char c) { return c; } /* usually nothing */
N int      narrow(long x) { return (int)x; }                  /* free: w0 IS x0's low half */

/* ======================= 5. LOADS AND STORES ============================ */

/* arm64 is load/store: arithmetic never touches memory directly. Every
   addressing mode below is one instruction. */
N int  load_plain(const int *p)            { return *p; }      /* ldr w0,[x0] */
N int  load_offset(const int *p)           { return p[3]; }    /* ldr w0,[x0,#12] */
N int  load_indexed(const int *p, long i)  { return p[i]; }    /* ldr w0,[x0,x1,lsl #2] */
N void store_offset(int *p, int v)         { p[2] = v; }       /* str w1,[x0,#8] */

/* Size and signedness are properties of the LOAD, not of the register. */
N int  load_byte_signed(const signed char *p)   { return *p; }  /* ldrsb */
N int  load_byte_unsigned(const unsigned char *p){ return *p; } /* ldrb  */
N int  load_half_signed(const short *p)          { return *p; } /* ldrsh */
N long load_word_signed(const int *p)            { return *p; } /* ldrsw */

/* Two registers in one instruction: how prologues, epilogues and struct copies
   move 16 bytes at a time. */
struct pair { long a, b; };
N struct pair load_pair(const struct pair *p) { return *p; }    /* ldp / stp */

/* Post-increment: load, then advance the pointer. This is the instruction that
   makes chapter 01's loop counter disappear. */
N int sum_walk(const int *p, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += *p++;                      /* ldr w,[x],#4 */
    return s;
}

/* Unscaled/unaligned access uses the 'u' forms: ldur/stur. */
N int load_unaligned(const char *p) {
    int v; __builtin_memcpy(&v, p + 1, sizeof v); return v;     /* ldur */
}

/* Addresses of globals are not constants either: the linker fills in a PAGE and
   a PAGEOFF, so taking an address is adrp + add. */
int global_int;
N int *address_of_global(void) { return &global_int; }          /* adrp + add */

/* ======================= 6. CONDITIONAL EXECUTION ====================== */

/* The flags, once set, can be consumed WITHOUT branching. These are the
   instructions behind chapter 01's vanishing if/else. */
N int select_max(int a, int b)   { return a > b ? a : b; }   /* csel  */
N int set_if_equal(int a, int b) { return a == b; }          /* cset  */
N int select_inc(int a, int b)   { return a > b ? b + 1 : b; }/* csinc */
N int select_neg(int a, int b)   { return a > 0 ? b : -b; }  /* csneg */

/* Two conditions, one flag register: ccmp performs the second compare only if
   the first did not already decide the answer. Short-circuit, branchlessly. */
N int both_conditions(int a, int b) { return a == 1 && b == 2; }  /* ccmp */

/* ======================= 7. BRANCHES =================================== */

/* These need a real branch in the taken path to show the branch instruction --
   if the result is just a value, the compiler uses cset and never branches. */
int sink(void);

N int branch_if_zero(const int *p) { return p ? *p : -1; }        /* cbz  */
N int branch_if_bit_set(int x) { if (x & (1 << 20)) return sink(); return 0; }
                                                                  /* tbnz w0,#20 */
N int branch_if_negative(int x) { if (x < 0) return sink(); return 0; }
                                                   /* tbnz w0,#31 -- the sign bit */
N int branch_if_mask(int x) { if (x & 9) return sink(); return 0; }/* tst + b.eq */
N int branch_if_neg_const(int x) { if (x == -5) return sink(); return 0; }
                                                   /* cmn w0,#5: compare-negative */

/* bl = branch-with-link (a call): the return address goes in x30/lr.
   ret = branch to lr.  blr = call the address in a register (chapter 06).  */
int callee(int);
N int call_direct(int x)   { return callee(x) + 1; }         /* bl  ... ret */
N int call_tail(int x)     { return callee(x); }             /* b: a TAIL call */
N int call_indirect(int (*f)(int), int x) { return f(x); }   /* br / blr */

/* ======================= 8. THE ODD ONES =============================== */

N void crash(void) { __builtin_trap(); }        /* brk #1: a deliberate fault */
N void nothing(void) { }                        /* ret alone */
