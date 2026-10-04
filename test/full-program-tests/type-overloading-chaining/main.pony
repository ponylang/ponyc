use @pony_exitcode[None](code: I32)

class Overloaded
  var _s: USize = 0

  fun ref apply(s: String): USize => _s = _s + 1; _s
  fun ref apply(n: USize): USize => _s = _s + n; _s

  fun get(): USize => _s

actor Main
  new create(env: Env) =>
    let o = Overloaded
    o.>apply("hello")
      .>apply(USize(10))

    if o.get() == 11 then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
