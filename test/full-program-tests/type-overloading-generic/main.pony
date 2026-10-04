use @pony_exitcode[None](code: I32)

interface val Readable
  fun read(): String

class MyReadable is Readable
  let _s: String
  new val create(s: String) => _s = s
  fun read(): String => _s

class Foo[A: Readable val]
  fun apply(a: A): USize => 1
  fun apply(n: USize): USize => 2

actor Main
  new create(env: Env) =>
    let f = Foo[MyReadable val]
    try
      if f(MyReadable("hello")) != 1 then error end
      if f(USize(42)) != 2 then error end
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
