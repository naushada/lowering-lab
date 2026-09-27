/* Before looking at what the compiler generates, write it by hand. This is the
   whole idea of a coroutine: a function that can RETURN IN THE MIDDLE and be
   resumed later. It needs exactly two things:
     1. the locals must not live on the stack (the stack frame will be gone),
     2. there must be a record of WHERE to resume.
   So: move the locals into a struct, and add a state field. */

struct gen {
    int state;      /* where to resume: this replaces the program counter */
    int i, n;       /* what were locals; they now live in the object      */
    int value;      /* what a "yield" hands back                          */
};

void gen_init(struct gen *g, int n) { g->state = 0; g->n = n; }

/* One function, resumable. Note there is no stack discipline here at all: the
   "suspend" is an ordinary `return`, and the "resume" is an ordinary call.
   That is why this model needs no extra stacks and scales to millions. */
int gen_next(struct gen *g) {
    switch (g->state) {                 /* the jump table IS the resume point */
        case 0: goto entry;
        case 1: goto resume1;
        case 2: return 0;               /* done */
    }
entry:
    for (g->i = 0; g->i < g->n; g->i++) {
        g->value = g->i * g->i;
        g->state = 1;
        return 1;                       /* <-- "co_yield": suspend by returning */
    resume1:;                           /* <-- and we come back HERE */
    }
    g->state = 2;
    return 0;
}

/* The same computation as a plain loop, for comparison. */
int sum_direct(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += i * i;
    return s;
}
int sum_via_gen(int n) {
    struct gen g;
    gen_init(&g, n);
    int s = 0;
    while (gen_next(&g)) s += g.value;
    return s;
}
