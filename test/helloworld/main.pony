trait Foo val
  fun box test(): I32 => 0

class Bar is Foo
  fun box test(): I32 => 1

class Baz is Foo
  fun box test(): I32 => 2

class Throw
  fun box throw(test: Bool): Bool ? =>
    if test then test else error end

actor Main
  new create(env: Env) =>
    var t: Foo = Bar
    @printf[I32]("%d\n"._cstring(), t.test())

    t = Baz
    @printf[I32]("%d\n"._cstring(), t.test())

    var p = @printf[I32]("%d\n"._cstring(), env.args.length() + 100)
    @printf[I32]("%d\n"._cstring(), p)

    var num = for i in Range[U64](0, env.args.length()) do
      if i == 3 then
        break 100
      elseif i == 0 then
        i
      else
        continue
      end
    else
      99
    end
    @printf[I32]("%zu\n"._cstring(), num)

    for s in env.args.values() do
      env.stdout.print(s)
    end

    try
      error
    else
      @printf[I32]("caught error\n"._cstring())
    end

    try
      var throw = Throw
      throw.throw(False)
      @printf[I32]("all ok\n"._cstring())
    else
      @printf[I32]("caught nested error\n"._cstring())
    then
      @printf[I32]("then clause\n"._cstring())
    end
