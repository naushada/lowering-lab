/* A struct is an offset table applied at compile time. At runtime there are
   only addresses. `p->c` is `ldr [x0, #8]` -- the name 'c' does not survive. */
#include <stdio.h>
#include <stddef.h>

struct packed_well { long a; int b; int c; };        /* 16 bytes, no holes */
struct padded      { char a; long b; char c; };      /* 24 bytes, 14 wasted */
struct reordered   { long b; char a; char c; };      /* 16 bytes, same fields  */

int  get_c(const struct packed_well *p) { return p->c; }
void set_c(struct packed_well *p, int v) { p->c = v; }
long get_b(const struct padded *p)      { return p->b; }

/* Nested structs are flattened: the offsets just add up. */
struct inner { int x, y; };
struct outer { int tag; struct inner in; };
int nested(const struct outer *o) { return o->in.y; }

/* Bitfields: the compiler emits shifts and masks. Convenient, not free. */
struct flags { unsigned a : 1; unsigned b : 3; unsigned c : 12; };
unsigned get_bf(const struct flags *f) { return f->b; }
void     set_bf(struct flags *f, unsigned v) { f->b = v; }

/* A union is one piece of storage with two names and no tag. The language does
   not remember which one you wrote; that is what the tag field is for. */
union u { float f; unsigned bits; };
unsigned float_bits(float f) { union u u = { .f = f }; return u.bits; }

int main(void) {
    printf("packed_well %2zu  (a@%zu b@%zu c@%zu)\n", sizeof(struct packed_well),
           offsetof(struct packed_well,a), offsetof(struct packed_well,b), offsetof(struct packed_well,c));
    printf("padded      %2zu  (a@%zu b@%zu c@%zu)  <- %zu bytes of holes\n", sizeof(struct padded),
           offsetof(struct padded,a), offsetof(struct padded,b), offsetof(struct padded,c),
           sizeof(struct padded) - (1 + 8 + 1));
    printf("reordered   %2zu  (b@%zu a@%zu c@%zu)  <- same fields, sorted by size\n", sizeof(struct reordered),
           offsetof(struct reordered,b), offsetof(struct reordered,a), offsetof(struct reordered,c));
    return 0;
}
