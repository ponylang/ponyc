use "cli"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf("echo", "A sample echo program", [
          OptionSpec.bool("upper", "Uppercase words"
            where short' = 'U', default' = false)
        ], [
          ArgSpec.string_seq("words", "The words to echo")
        ])?.>add_help()?
      else
        env.exitcode(-1)  // some kind of coding error
        return
      end

    let cmd =
      match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
          ch.print_help(env.out)
          env.exitcode(0)
          return
      | let se: SyntaxError =>
          env.out.print(se.string())
          env.exitcode(1)
          return
      end

    let upper = cmd.option("upper").bool()
    let words = cmd.arg("words").string_seq()
    for word in words.values() do
      env.out.write(if upper then word.upper() else word end + " ")
    end
    env.out.print("")
