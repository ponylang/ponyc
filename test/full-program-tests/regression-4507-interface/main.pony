// Regression test for https://github.com/ponylang/ponyc/issues/4507
//
// Matching a boxed tuple (via Any val) against a pattern with an interface-
// typed element segfaulted for the same reason as the union case: the codegen
// loaded the unboxed primitive as a pointer.

actor Main
  new create(env: Env) =>
    let x: Any val = ("a", U8(1))
    match x
    | (let a: String, let b: Stringable) =>
      if b.string() != "1" then
        env.exitcode(1)
      end
    else
      env.exitcode(1)
    end
