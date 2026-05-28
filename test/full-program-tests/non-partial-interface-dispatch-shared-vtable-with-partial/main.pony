// Regression test for the interaction between #5002 (error-flag returns)
// and shared vtable indices for partial and non-partial methods.
//
// The painter assigns vtable indices by mangled name, and Pony's mangling
// does not include partiality, so a partial `apply(T): U ?` and a
// non-partial `apply(T): U` with otherwise-identical signatures share a
// vtable slot on any concrete type that implements both. Codegen wraps
// the slot's return as `{T, i1}` (the partial calling convention) so the
// slot has one shape. The non-partial interface dispatch site has to
// unwrap that wrapped return before using it.
//
// Before the fix, the non-partial call site used the unwrapped function
// type. For small returns the bug was invisible (the register layouts
// the caller and callee disagreed on happened to overlap), but for
// large returns the disagreement reliably manifested as a crash — on
// x86_64 the wrapper expected an sret destination buffer in `rdi`, the
// caller passed the lambda's `this` pointer there, and the wrapper
// wrote the return into read-only memory.
//
// The 3-tuple of `String val` here is large enough to make the caller
// and callee ABIs noticeably disagree (each `String val` is a pointer,
// so the unwrapped 24-byte aggregate is already past the 16-byte
// two-register threshold; the wrapped form adds an i1 flag on top).

actor Main
  new create(env: Env) =>
    // Reach the partial lambda interface
    // `{(String val): (String val, String val, String val) ?}`. The
    // painter then assigns its `apply` the same vtable index as our
    // non-partial lambda's `apply` (same mangled name), forcing the
    // non-partial slot to be wrapped.
    _call_partial({(s: String val): (String val, String val, String val) ? =>
      if s == "fail" then error end
      (s, s, s)
    })

    // Dispatch through the non-partial lambda interface. The slot is
    // wrapped, so the call site has to retype the call and unwrap the
    // result; without that, the wrapper writes the result into the
    // lambda receiver pointer and segfaults.
    let f: {(String val): (String val, String val, String val)} box =
      {(s: String val): (String val, String val, String val) => (s, s, s) }

    (let a, let b, let c) = f("hello")

    if (a != "hello") or (b != "hello") or (c != "hello") then
      env.exitcode(1)
    end

  fun _call_partial(
    p: {(String val): (String val, String val, String val) ?} box)
  =>
    try
      (let x, let y, let z) = p("")?
      if x.size() == 0 then None end
    end
