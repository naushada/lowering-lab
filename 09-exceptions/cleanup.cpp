// What makes unwinding hard is not the jump -- it is that every destructor
// between throw and catch must run, in reverse order, while the stack is being
// dismantled. The compiler emits, in a separate section, a TABLE mapping ranges
// of code addresses to the cleanup that applies there. `make raw` to see it:
//     grep -A30 GCC_except_table out/O1/cleanup.s
#include <cstdio>

struct Guard {
    const char *name;
    explicit Guard(const char *n) : name(n) { printf("  +%s\n", name); }
    ~Guard() { printf("  -%s\n", name); }          // must run even when unwinding
};

void boom();                                        // may throw; defined elsewhere

void two_guards() {
    Guard a("a");
    Guard b("b");
    boom();                                         // if this throws: -b then -a
}

// noexcept is a PROMISE, and the compiler enforces it by calling std::terminate
// instead of unwinding. Look for __clang_call_terminate in the output.
void promises() noexcept {
    Guard g("g");
    boom();
}

// A function with no destructors and no catch needs no cleanup code at all --
// but it still needs an unwind TABLE entry, so the unwinder knows to keep going.
int passthrough(int x) { boom(); return x; }
