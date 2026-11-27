// An object literal in Main provides l.Foo, whose default has a type error, and
// Main has its own type error after the literal. The test expects only the
// error in l.Foo: an error inside an object literal's members ends the pass.
use l = "lib"
actor Main
  new create(env: Env) =>
    let o = object is l.Foo end
    let x: U8 = "main"
