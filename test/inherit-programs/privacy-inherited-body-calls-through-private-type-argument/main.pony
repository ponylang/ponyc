// C is T[_M], with _M and _Q private to main. T's bar(), written in lib, calls
// make() and string() on them. The private names come from the type argument,
// not from lib's text, so this compiles; expects q.
use "lib"
primitive _Q is Stringable
  fun string(): String iso^ => "q".clone()
primitive _M
  fun make(): _Q => _Q
class C is T[_M]
actor Main
  new create(env: Env) => env.out.print(C.bar(_M))
