/* How a 64-bit register divides up -- demonstrated, not asserted.
   Inline asm is used so that each access is exactly the instruction named,
   with nothing for the optimiser to reinterpret.  `make run` */

#include <stdio.h>
#include <stdint.h>

int main(void) {
    const uint64_t v = 0x1122334455667788ULL;
    uint64_t out;

    puts("value in x1:        0x1122334455667788\n");

    /* --- the two integer views ------------------------------------------ */
    __asm__("mov %x0, %x1" : "=r"(out) : "r"(v));
    printf("mov x0, x1          0x%016llx   the whole 64 bits\n", out);

    /* Writing the w half ZEROES the upper 32 bits. This is architectural, not
       an optimisation -- which is why narrowing a long to an int is free. */
    out = 0xFFFFFFFFFFFFFFFFULL;
    __asm__("mov %w0, %w1" : "+r"(out) : "r"(v));
    printf("mov w0, w1          0x%016llx   low 32 kept, HIGH 32 ZEROED\n", out);

    /* There is no register name for bits 63..32, so reaching them is a shift. */
    __asm__("lsr %x0, %x1, #32" : "=r"(out) : "r"(v));
    printf("lsr x0, x1, #32     0x%016llx   the only way to the high half\n\n", out);

    /* --- 16-bit granularity, place 1: building constants ---------------- */
    __asm__("movz %x0, #0x7788\n\t"
            "movk %x0, #0x5566, lsl #16\n\t"
            "movk %x0, #0x3344, lsl #32\n\t"
            "movk %x0, #0x1122, lsl #48" : "=r"(out));
    printf("movz + 3x movk      0x%016llx   four 16-bit slots\n", out);

    /* --- 16-bit granularity, place 2: memory accesses ------------------- */
    uint64_t mem = v;
    __asm__("ldrh %w0, [%1]" : "=r"(out) : "r"(&mem) : "memory");
    printf("ldrh w0, [x1]       0x%016llx   16-bit load, zero-extended\n", out);
    __asm__("ldrb %w0, [%1]" : "=r"(out) : "r"(&mem) : "memory");
    printf("ldrb w0, [x1]       0x%016llx   8-bit load\n", out);

    /* Sign extension is only visible when the top bit of the loaded field is
       set, so this one needs a value ending in 0x8899, not 0x7788. */
    uint64_t neg = 0x1122334455668899ULL;
    printf("\nvalue in x1:        0x1122334455668899   (low halfword 0x8899)\n");
    __asm__("ldrh %w0, [%1]" : "=r"(out) : "r"(&neg) : "memory");
    printf("ldrh w0, [x1]       0x%016llx   zero-extended: top bit ignored\n", out);
    __asm__("ldrsh %x0, [%1]" : "=r"(out) : "r"(&neg) : "memory");
    printf("ldrsh x0, [x1]      0x%016llx   SIGN-extended: top bit replicated\n\n", out);

    /* --- 16-bit granularity, place 3: vector lanes --------------------- */
    printf("On arm64 there is no 'ax'/'al'. Sub-32-bit access comes from the\n"
           "LOAD/STORE instruction or from a vector lane -- never from a\n"
           "register name. See docs/register-views.md\n");
    return 0;
}
