// An object is a struct. A method is a function whose first argument is the
// object's address. `this` is not magic -- it is x0.
//
// The only thing C++ adds that needs runtime support is *virtual* dispatch, and
// it needs exactly one thing: an indirect call through a table of function
// pointers. Read the mangled names in the output; `make dis` demangles them.

struct Plain {
    int v;
    int get() const { return v; }           // -> _ZNK5Plain3getEv(Plain* this)
    int add(int k) const { return v + k; }
};

int use_plain(const Plain *p) { return p->get() + p->add(3); }   // direct calls,
                                                                // usually inlined

struct Base {
    int v;
    virtual int  f() const { return v; }    // slot 0 (after the two std slots)
    virtual int  g() const { return v * 2; }// slot 1
    virtual ~Base() = default;
};

struct Derived : Base {
    int w;
    int f() const override { return v + w; }
};

struct Final final : Base {
    int f() const override { return 42; }
};

// The whole point: this function does not know which f() it calls. Two loads and
// one indirect branch. That is polymorphism, complete.
int call_virtual(const Base *b) { return b->f(); }

// Two virtual calls on the same object: does the vptr get loaded twice?
// (It may not -- the compiler knows the type cannot change mid-function.)
int call_twice(const Base *b) { return b->f() + b->g(); }

// DEVIRTUALISATION. Here the dynamic type is known exactly, so the indirect call
// is replaced by a direct one -- and then inlined to a constant.
int call_known_type() { Derived d; d.v = 1; d.w = 2; return d.f(); }

// `final` lets the compiler prove there is no further override.
int call_final(const Final *f) { return f->f(); }

// Calling a virtual function through a reference to the exact type, constructed
// locally: the classic case where the vtable disappears completely.
int call_local() { Final f; f.v = 9; return f.f(); }
