// Two ways to report failure through four stack frames. The question this file
// answers by measurement: what does each one cost when NOTHING goes wrong?
//
//   *_thr : C++ exceptions      -> the happy path carries no checks at all
//   *_ec  : returned error code -> one test-and-branch per frame, forever
//
// `make table` and compare the instruction counts side by side.
#include <stdexcept>

#define NOINLINE __attribute__((noinline))

// ---- exceptions -----------------------------------------------------------
NOINLINE int lvl3_thr(int x) { if (x < 0) throw std::runtime_error("negative"); return x + 1; }
NOINLINE int lvl2_thr(int x) { return lvl3_thr(x) + 1; }
NOINLINE int lvl1_thr(int x) { return lvl2_thr(x) + 1; }
NOINLINE int lvl0_thr(int x) { return lvl1_thr(x) + 1; }

// The intermediate frames contain NO error handling whatsoever. They do not even
// know an exception is possible.
int happy_thr(int x) { return lvl0_thr(x); }

// Only the frame that actually catches pays: __cxa_begin_catch / __cxa_end_catch
// and a landing pad the unwinder jumps to.
int caught_thr(int x) {
    try { return lvl0_thr(x); }
    catch (const std::exception &) { return -1; }
}

// ---- error codes ----------------------------------------------------------
NOINLINE int lvl3_ec(int x, int *out) { if (x < 0) return -1; *out = x + 1; return 0; }
NOINLINE int lvl2_ec(int x, int *out) { int t; if (lvl3_ec(x, &t)) return -1; *out = t + 1; return 0; }
NOINLINE int lvl1_ec(int x, int *out) { int t; if (lvl2_ec(x, &t)) return -1; *out = t + 1; return 0; }
NOINLINE int lvl0_ec(int x, int *out) { int t; if (lvl1_ec(x, &t)) return -1; *out = t + 1; return 0; }

int happy_ec(int x) { int out = 0; if (lvl0_ec(x, &out)) return -1; return out; }
