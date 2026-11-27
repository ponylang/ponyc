// Bar, in main, passes an alias of a union of lib's private _H and _H2 to Foo,
// whose literal calls name() on the captured A. Expects, for each member, the
// private lookup error and the couldn't-find error, all with the inherited-g
// note, matching what ordinary code reports for the same call.
use l = "lib"
class val Bar is l.Foo[l.PubH]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(l.MakeH()).show())
