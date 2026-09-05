use "cli"

actor Main
  """
  CLI entry point for pony-dep. Parses command-line arguments and dispatches
  to the requested subcommand.
  """
  new create(env: Env) =>
    if _is_version_flag(env.args) then
      env.out.print("pony-dep " + Version())
      return
    end

    let cs =
      try
        CommandSpec.parent(
          "pony-dep",
          "A dependency manager for Pony packages",
          [],
          [
            CommandSpec.leaf(
              "pack",
              "Create an archive from a project's source")?
            CommandSpec.leaf(
              "fetch",
              "Download and extract a package archive from a URL")?
            CommandSpec.leaf(
              "add",
              "Fetch a dependency, hash it, and record it")?
            CommandSpec.leaf(
              "remove",
              "Remove a dependency and its placed files")?
            CommandSpec.leaf(
              "clean",
              "Remove placed packages that nothing references")?
          ])? .> add_help()?
      else
        env.err.print("internal error: invalid command spec")
        env.exitcode(1)
        return
      end

    let cmd =
      match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        return
      | let se: SyntaxError =>
        env.err.print(se.string())
        env.exitcode(1)
        return
      end

    match cmd.spec().name()
    | "pack" => _not_implemented(env, "pack")
    | "fetch" => _not_implemented(env, "fetch")
    | "add" => _not_implemented(env, "add")
    | "remove" => _not_implemented(env, "remove")
    | "clean" => _not_implemented(env, "clean")
    else
      env.err.print("error: no subcommand specified")
      env.exitcode(1)
    end

  fun _is_version_flag(args: Array[String val] val): Bool =>
    try
      (args(1)? == "--version") or (args(1)? == "-V")
    else
      false
    end

  fun _not_implemented(env: Env, name: String) =>
    env.err.print(name + ": not yet implemented")
    env.exitcode(1)
