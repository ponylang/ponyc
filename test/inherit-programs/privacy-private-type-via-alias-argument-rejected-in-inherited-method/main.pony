// Bar provides l.Foo[l.PubH], where l.PubH is a public alias of lib's private
// _H, and Foo's default calls x.name(). The test expects the private-lookup
// error with a note that _H reaches the method through the type argument
// written in main.
use l = "lib"
class Bar is l.Foo[l.PubH]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(l.Box[l.PubH](l.MakeH()).show())
