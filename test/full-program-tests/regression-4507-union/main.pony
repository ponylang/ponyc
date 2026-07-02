// Regression test for https://github.com/ponylang/ponyc/issues/4507
//
// Matching a boxed tuple (via Any val) against a pattern with a union-typed
// element segfaulted because the codegen loaded the unboxed primitive as a
// pointer.

actor Main
  new create(env: Env) =>
    let x: Any val = ("a", U8(1))
    match x
    | (let a: String, let b: (U8 | U16)) =>
      if b.string() != "1" then
        env.exitcode(1)
      end
    else
      env.exitcode(1)
    end
