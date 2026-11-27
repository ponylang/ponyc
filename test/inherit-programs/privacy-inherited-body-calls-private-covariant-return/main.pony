// C overrides foo() to return _P, private to main. T's bar() and baz(), written
// in lib, call string() on the result. Those calls must compile in C's copies;
// expects pp.
use "lib"
primitive _P is Stringable
  fun string(): String iso^ => "p".clone()
  fun extra(): String => "x"
class C is T
  fun foo(): _P => _P
actor Main
  new create(env: Env) => env.out.print(C.bar() + C.baz())
