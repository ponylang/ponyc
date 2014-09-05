trait Foo val
  fun box test(): I32 => 0

class Bar is Foo
  fun box test(): I32 => 1

class Baz is Foo
  fun box test(): I32 => 2

actor Main
  new create(env: Env) =>
    var t: Foo = Bar
    @printf[I32]("%d\n"._cstring(), t.test())

    t = Baz
    @printf[I32]("%d\n"._cstring(), t.test())

    var p = @printf[I32]("%d\n"._cstring(), env.args.length() + 100)
    @printf[I32]("%d\n"._cstring(), p)

    for s in env.args.values() do
      env.stdout.print(s)
    end
