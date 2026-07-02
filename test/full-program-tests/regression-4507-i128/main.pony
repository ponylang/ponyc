// Regression test for https://github.com/ponylang/ponyc/issues/4507
//
// Tests a wider primitive type (I128) to exercise the alignment-aware boxing
// in box_field_with_descriptor.  I128 has 16-byte alignment on x86-64, so the
// value sits at offset 16 in the boxed struct rather than offset 8.

actor Main
  new create(env: Env) =>
    let x: Any val = ("a", I128(42))
    match x
    | (let a: String, let b: (I128 | U128)) =>
      if b.string() != "42" then
        env.exitcode(1)
      end
    else
      env.exitcode(1)
    end
