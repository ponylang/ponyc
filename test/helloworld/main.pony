actor Main
  new create(env: Env) =>
    var p = @printf[I32]("%d\n"._cstring(), env.args.length() + 100)
    @printf[I32]("%d\n"._cstring(), p)
    for s in env.args.values() do
      env.stdout.print(s)
    end
