use "cli"
use "files"

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
              "Create an archive from a project's source",
              [
                OptionSpec.bool(
                  "hash", "Print content hash to stdout", 'H', false)
              ],
              [
                ArgSpec.string("directory")
                ArgSpec.string("output")
              ])?
            CommandSpec.leaf(
              "fetch",
              "Fetch all dependencies from a config file",
              [],
              [
                ArgSpec.string("config-file")
                ArgSpec.string("directory")
              ])?
            CommandSpec.leaf(
              "add",
              "Fetch a dependency, hash it, and add it to config",
              [
                OptionSpec.string(
                  "type", "Fetch protocol" where short' = 't', default' = "par")
                OptionSpec.string(
                  "ref", "Version reference" where short' = 'r', default' = "")
                OptionSpec.string(
                  "documentation-url", "Documentation URL"
                  where default' = "")
              ],
              [
                ArgSpec.string("name")
                ArgSpec.string("url")
                ArgSpec.string("config-file")
              ])?
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

    let auth = FileAuth(env.root)

    match cmd.spec().name()
    | "pack" =>
      let dir = cmd.arg("directory").string()
      let out = cmd.arg("output").string()
      match \exhaustive\ Pack(
        auth, dir, out where hash = cmd.option("hash").bool())
      | let s: String val => env.out.print(s)
      | None => None
      | PackSourceNotFound =>
        env.err.print("error: cannot stat '" + dir + "'")
        env.exitcode(1)
      | PackSourceNotDirectory =>
        env.err.print("error: '" + dir + "' is not a directory")
        env.exitcode(1)
      | PackOutputInsideSource =>
        env.err.print(
          "error: output path cannot be inside the source directory")
        env.exitcode(1)
      | PackFailed =>
        env.err.print("error: pack failed for '" + dir + "'")
        env.exitcode(1)
      end
    | "fetch" =>
      ResolveDeps(
        env,
        cmd.arg("config-file").string(),
        _ResolveHandler(env),
        cmd.arg("directory").string())
    | "add" =>
      let ref_str = cmd.option("ref").string()
      let ref_opt: (String val | None) =
        if ref_str.size() == 0 then None else ref_str end
      let doc_str = cmd.option("documentation-url").string()
      let doc_url: (String val | None) =
        if doc_str.size() == 0 then None else doc_str end
      let config_path = cmd.arg("config-file").string()
      let config_dir = Path.dir(config_path)
      let config = ConfigAccess(auth, config_path)
      Add(
        env,
        config,
        _AddHandler(env),
        cmd.arg("name").string(),
        cmd.arg("url").string()
        where work_dir =
            if config_dir.size() == 0 then "."
            else config_dir
            end,
          dep_type = cmd.option("type").string(),
          ref_name = ref_opt,
          documentation_url = doc_url)
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

actor _AddHandler is AddNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  be add_failed(message: String val) =>
    _env.err.print("error: " + message)
    _env.exitcode(1)

  be add_succeeded() =>
    None
