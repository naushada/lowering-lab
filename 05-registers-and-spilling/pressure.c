/* The IR pretends there are infinitely many variables. The machine has about
   thirty registers, of which a function may freely use maybe fifteen.
   Register allocation is graph colouring: two values that are live at the same
   moment interfere and need different colours, and when the colours run out a
   value is SPILLED to a stack slot and reloaded later.

   The lesson of this file: the cost is not how many variables you declare, it
   is how LONG each one must stay alive -- and a function call is the wall that
   makes things stay alive. */

int barrier(void);      /* opaque: the optimiser cannot see through a call */
int f(int);

/* Four values, all short-lived. Zero stack traffic, no callee-saved registers,
   no prologue. */
int low(int a, int b, int c, int d) {
    int w = a + b, x = b + c, y = c + d, z = d + a;
    return w * x + y * z;
}

/* Sixteen values loaded from memory (so they cannot be constant-folded away),
   all needed AFTER a call, each with a different weight (so none is redundant).
   arm64 has ten callee-saved integer registers; sixteen values do not fit.
   Read the prologue and count: all ten are saved, and the rest go to the stack. */
int forced_spills(const int *s) {
    int v0=s[0], v1=s[1], v2 =s[2],  v3 =s[3],  v4 =s[4],  v5 =s[5],  v6 =s[6],  v7 =s[7];
    int v8=s[8], v9=s[9], v10=s[10], v11=s[11], v12=s[12], v13=s[13], v14=s[14], v15=s[15];
    int k = barrier();
    return v0*1 + v1*2 + v2*3 + v3*4 + v4*5 + v5*6 + v6*7 + v7*8
         + v8*9 + v9*10 + v10*11 + v11*12 + v12*13 + v13*14 + v14*15 + v15*16 + k;
}

/* The SAME sixteen loads, the same weights, the same call -- but each value is
   folded into the accumulator before the next is loaded. Nothing overlaps,
   so it needs a couple of registers and spills nothing. Identical arithmetic,
   different lifetimes, and the register allocator's job becomes trivial. */
int short_lived(const int *s) {
    int k = barrier();
    int acc = 0;
    acc += s[0]*1;  acc += s[1]*2;  acc += s[2]*3;  acc += s[3]*4;
    acc += s[4]*5;  acc += s[5]*6;  acc += s[6]*7;  acc += s[7]*8;
    acc += s[8]*9;  acc += s[9]*10; acc += s[10]*11; acc += s[11]*12;
    acc += s[12]*13; acc += s[13]*14; acc += s[14]*15; acc += s[15]*16;
    return acc + k;
}

/* Every result must be held somewhere safe while the next call runs, so the
   prologue grows one callee-saved register per surviving value. This is a real
   component of the cost of a function call, and it is invisible in the source. */
int across_calls(int a, int b, int c, int d) {
    int p = f(a), q = f(b), r = f(c), s = f(d);
    return p + q*2 + r*3 + s*4;
}

/* Taking an address defeats registers by definition: the value must have a real
   location, so it gets a stack slot whether it is hot or not. "Address taken"
   is one of the oldest reasons for slow code, and the reason `restrict` and
   escape analysis exist. */
int address_taken(int a) {
    int x = a * 3;
    int *p = &x;
    return *p + barrier();
}
