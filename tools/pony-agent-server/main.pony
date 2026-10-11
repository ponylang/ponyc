use "cli"
use "files"
use "path:../lib/ponylang/pony_compiler/"
use "pony_compiler"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "pony-agent-server",
          "Query server for AI agents working with Pony code",
          [
            OptionSpec.bool(
              "version",
              "Print version and exit"
              where short' = 'V', default' = false)
            OptionSpec.bool(
              "queries",
              "Print supported queries and exit"
              where short' = 'Q', default' = false)
            OptionSpec.string_seq(
              "define",
              "Compile-time define (repeatable, e.g. -D openssl_3.0.x)"
              where short' = 'D')
          ],
          [
            ArgSpec.string(
              "package",
              "Path to the Pony package to compile"
              where default' = "")
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

    if cmd.option("version").bool() then
      env.out.print("pony-agent-server " + Version())
      return
    end

    if cmd.option("queries").bool() then
      _print_queries(env)
      return
    end

    let package_path = cmd.arg("package").string()
    if package_path.size() == 0 then
      env.err.print("error: package path is required")
      env.err.print(
        "usage: pony-agent-server <package> [-D define]...")
      env.err.print(
        "run with --help for options" +
        " or --queries for the query reference")
      env.exitcode(1)
      return
    end

    let file_path = FilePath(FileAuth(env.root), package_path)

    let argv0 =
      try env.args(0)?
      else ""
      end

    let defines = cmd.option("define").string_seq()
    let user_flags: Array[String val] val =
      recover val
        let flags = Array[String val]
        for d in defines.values() do
          flags.push(d)
        end
        consume flags
      end

    let server = AgentServer(env, argv0, user_flags)
    server.compile(file_path)

  fun _print_queries(env: Env) =>
    env.out.print(QueryHelp())

primitive \nodoc\ QueryHelp
  fun apply(): String =>
    "pony-agent-server queries\n" +
    "\n" +
    "The server reads one JSON object per line from stdin\n" +
    "and writes one JSON response per line to stdout.\n" +
    "Every query has a \"query\" field. An optional \"id\"\n" +
    "field (string or integer) is echoed back.\n" +
    "\n" +
    "inspect\n" +
    "  Type and definition at a source position.\n" +
    "  {\"query\":\"inspect\",\n" +
    "   \"file\":\"...\", \"line\":N, \"column\":N}\n" +
    "\n" +
    "scope\n" +
    "  Names visible at a source position.\n" +
    "  {\"query\":\"scope\",\n" +
    "   \"file\":\"...\", \"line\":N, \"column\":N}\n" +
    "\n" +
    "exports\n" +
    "  Public types in a package.\n" +
    "  {\"query\":\"exports\", \"package\":\"...\"}\n" +
    "\n" +
    "type_api\n" +
    "  Callable methods for a type at a capability.\n" +
    "  {\"query\":\"type_api\",\n" +
    "   \"type\":\"...\", \"cap\":\"ref\"}\n" +
    "  {\"query\":\"type_api\",\n" +
    "   \"file\":\"...\", \"line\":N, \"column\":N}\n" +
    "\n" +
    "check\n" +
    "  Subtype check between two types.\n" +
    "  {\"query\":\"check\",\n" +
    "   \"source\":\"...\", \"target\":\"...\"}\n" +
    "  Types can include a capability:\n" +
    "    \"String val\", \"Collector ref\".\n" +
    "  Without one, the type's default is used.\n" +
    "\n" +
    "implementors\n" +
    "  Types that implement a trait or interface.\n" +
    "  {\"query\":\"implementors\", \"type\":\"...\"}\n" +
    "\n" +
    "errors\n" +
    "  Errors from the last compile or reload.\n" +
    "  {\"query\":\"errors\"}\n" +
    "\n" +
    "reload\n" +
    "  Recompile the package.\n" +
    "  {\"query\":\"reload\"}\n" +
    "\n" +
    "status\n" +
    "  Whether the server has a compiled program.\n" +
    "  {\"query\":\"status\"}\n" +
    "\n" +
    "Error responses have an \"error\" field\n" +
    "instead of query-specific fields."
