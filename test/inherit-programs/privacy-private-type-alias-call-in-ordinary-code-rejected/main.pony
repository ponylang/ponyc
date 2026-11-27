// main calls a method through its alias of lib's private _H in ordinary code.
// Expects one private lookup error, at main's call, and none from lib's trait
// Foo, which nothing in main provides.
use l = "lib"
actor Main
  new create(env: Env) =>
    let x: l.PubH = l.MakeH()
    env.out.print(x.name())
