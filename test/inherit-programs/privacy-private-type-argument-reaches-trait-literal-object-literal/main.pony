// An object literal in main provides lib's Foo with an alias of lib's private
// _H. Foo's literal calls name() on the captured A. Expects the private lookup
// error with notes for the object literal's inherited g and the type argument.
use l = "lib"
actor Main
  new create(env: Env) =>
    let o = object val is l.Foo[l.PubH] end
    env.out.print(o.g(l.MakeH()).show())
