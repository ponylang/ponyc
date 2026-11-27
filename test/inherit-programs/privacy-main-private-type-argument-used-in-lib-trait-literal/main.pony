// Bar in main is lib's Foo[_M] with main's private _M. Foo.g() builds a
// literal that calls x.name() on the _M. The program must print "main._M":
// a type argument's members may be called from another package's literal.
use l = "lib"
primitive _M
  fun name(): String => "main._M"
class val Bar is l.Foo[_M]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(_M).show())
