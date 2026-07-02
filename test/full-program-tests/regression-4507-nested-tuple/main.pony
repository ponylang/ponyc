// Regression test for https://github.com/ponylang/ponyc/issues/4507
//
// Tests that an inline nested tuple in a boxed tuple is correctly boxed when
// captured as a pointer type (Any val).  The nested tuple has an even type_id
// with (type_id & 3) == 2, exercising the tuple branch of the boxing path.

actor Main
  new create(env: Env) =>
    let inner: (U8, U16) = (U8(1), U16(2))
    let x: Any val = ("a", inner)
    match x
    | (let a: String, let b: Any val) =>
      match b
      | (let c: U8, let d: U16) =>
        if (c == 1) and (d == 2) then
          return
        end
      end
    end

    env.exitcode(1)
