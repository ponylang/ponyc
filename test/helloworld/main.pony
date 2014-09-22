trait Foo
  fun box test(): I32 => 0

class Bar is Foo
  fun box test(): I32 => 1

class Baz is Foo

class Throw
  fun box throw(test: Bool): Bool ? =>
    if test then test else error end

actor Test
  var _main: Main

  new create(m: Main) =>
    _main = m

actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env

    for s in env.args.values() do
      env.stdout.print(s)
    end

    Test(this)
    var t: Foo = Bar
    @printf[I32]("%d\n".cstring(), t.test())

    t = Baz
    @printf[I32]("%d\n".cstring(), t.test())

    var zero: U64 = 0
    var p = @printf[I32]("%d\n".cstring(), env.args.length() % zero)
    @printf[I32]("%d\n".cstring(), p)

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

    @printf[I32]("%d\n".cstring(), num.i32())

    try
      error
    else
      @printf[I32]("caught error\n".cstring())
    end

    try
      var throw = Throw
      throw.throw(False)
      @printf[I32]("all ok\n".cstring())
    else
      @printf[I32]("caught nested error\n".cstring())
    then
      @printf[I32]("then clause\n".cstring())
    end
