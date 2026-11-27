// T.mk, in t.pony, returns an object literal whose method matches a captured
// value with one case and no else. C, in main.pony, inherits mk. The program
// must compile and print ok.
class C is T
  new create() => None
  fun v(): U8 => 1

actor Main
  new create(env: Env) =>
    C.mk()()
    env.out.print("ok")
