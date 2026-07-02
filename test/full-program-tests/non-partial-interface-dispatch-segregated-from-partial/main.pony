// Regression test for the interaction between #5002 (error-flag returns)
// and partial vs non-partial vtable dispatch.
//
// Pony's mangling now encodes partiality (a trailing `?`), so a partial
// `apply(T): U ?` and a non-partial `apply(T): U` get distinct mangled
// names, distinct painter colours, and therefore distinct vtable slots.
// The partial slot returns `{T, i1}` (the partial calling convention) and
// the non-partial slot returns plain `T`. A non-partial concrete reached
// through a partial interface fills the partial slot with a forwarding
// wrapper (genfun_forward) that returns `{T, i1}`; each call site dispatches
// through the slot matching its own partiality, so no caller-side unwrap of
// a non-partial result is needed.
//
// Historically (before partiality entered the mangled name) the two shared
// one slot, which was forced to the `{T, i1}` shape, and the non-partial
// call site had to retype-and-unwrap. A bug in that unwrap path crashed for
// large returns: on x86_64 the wrapper expected an sret destination buffer
// in `rdi`, the caller passed the lambda's `this` pointer there, and the
// wrapper wrote the return into read-only memory. Segregating the slots
// removes that class of bug by construction; this test guards against
// regressions in both the partial and non-partial dispatch paths.
//
// The 3-tuple of `String val` is large enough that the partial (`{T, i1}`)
// and non-partial (`T`) ABIs differ noticeably (each `String val` is a
// pointer, so the 24-byte aggregate is already past the 16-byte
// two-register threshold; the partial form adds an i1 flag on top).

actor Main
  new create(env: Env) =>
    // Reach the partial lambda interface
    // `{(String val): (String val, String val, String val) ?}`. Its `apply`
    // gets its own vtable slot (partial mangled name), returning `{T, i1}`.
    _call_partial({(s: String val): (String val, String val, String val) ? =>
      if s == "fail" then error end
      (s, s, s)
    })

    // Dispatch through the non-partial lambda interface. This uses the
    // separate non-partial slot, which returns plain `T` — no unwrap. The
    // historical bug wrote the result into the lambda receiver pointer and
    // segfaulted; segregated slots make that impossible.
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
