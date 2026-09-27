// The same generator, with the compiler doing the transformation. `co_yield` is
// not a library call -- the front end rewrites the whole function into a state
// machine, exactly like manual.c, and puts the locals in a heap-allocated FRAME.
//
// What to look for in the output:
//   counter(int)                     the "ramp": allocates the frame and returns
//   counter(int) (.resume)           the state machine  <- the real body
//   counter(int) (.destroy)          frees the frame and runs destructors
//   operator new                     the frame allocation
#include <coroutine>
#include <cstdio>

struct Gen {
    struct promise_type {
        int value;
        Gen get_return_object() {
            return Gen{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int v) { value = v; return {}; }
        void return_void() {}
        void unhandled_exception() { __builtin_trap(); }
    };

    std::coroutine_handle<promise_type> h;
    explicit Gen(std::coroutine_handle<promise_type> handle) : h(handle) {}
    ~Gen() { if (h) h.destroy(); }
    Gen(const Gen &) = delete;
    Gen(Gen &&o) : h(o.h) { o.h = {}; }

    bool next() { h.resume(); return !h.done(); }
    int  value() const { return h.promise().value; }
};

// Three suspension points' worth of state, and not one line of state machine
// written by hand.
Gen counter(int n) {
    for (int i = 0; i < n; i++)
        co_yield i * i;
}

int sum_via_coro(int n) {
    Gen g = counter(n);
    int s = 0;
    while (g.next()) s += g.value();
    return s;
}

int sum_direct(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += i * i;
    return s;
}

int main() {
    printf("coro %d   direct %d\n", sum_via_coro(10), sum_direct(10));
}
