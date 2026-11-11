actor \nodoc\ Main
  new create(env: Env) =>
    try
      match env.args(1)?
      | "float" => env.out.print(_Sum[F64](3, 2).string())
      | "integer" => env.out.print(_Sum[I64](3, 2).string())
      | "defaults" =>
        env.out.print(_First.sum(3, 2).string())
        env.out.print(_Second.sum(3, 2).string())
      else
        env.exitcode(1)
      end
    else
      env.exitcode(1)
    end
