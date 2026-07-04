use "cli"
use "path:../lib/ponylang/pony_compiler/"
use "files"
use "pony_compiler"

// FFI declarations for setting stdout to binary mode on Windows.
// The LSP base protocol uses explicit \r\n in headers; Windows text mode
// translates \n to \r\n, producing \r\r\n on the wire which breaks clients.
use @pony_os_stdout[Pointer[None]]()
use @_fileno[I32](stream: Pointer[None] tag) if windows
use @_setmode[I32](fd: I32, mode: I32) if windows

actor Main
  """
  CLI entry point for pony-lsp. Parses command-line arguments, then starts the
  language server, which communicates with the editor over stdin/stdout.
  """
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "pony-lsp",
          "The Pony language server",
          [
            OptionSpec.bool(
              "version",
              "Print version and exit"
              where short' = 'V', default' = false)
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

    // Handle --version
    if cmd.option("version").bool() then
      env.out.print("pony-lsp " + Version())
      return
    end

    // No CLI action requested: start the language server.
    ifdef windows then
      @_setmode(@_fileno(@pony_os_stdout()), 0x8000)
    end
    let channel = Stdio(env.out, env.input)
    let pony_path =
      match \exhaustive\ PonyPath(env)
      | let p: String => p
      | None => ""
      end
    let argv0 =
      try
        env.args(0)?
      else
        ""
      end
    let language_server =
      LanguageServer(
        channel,
        env,
        PonyCompiler(pony_path, argv0))
    // at this point the server should listen to incoming messages via stdin
