use "../../ast"
use "term"
use "cli"
use "files"

actor CompilerActor

  var package: (Package | None) = None

  be compile(out: OutStream, path: FilePath, search_paths: ReadSeq[String] val = []) =>
    match Compiler.compile(path, search_paths)
    | let p: Program =>
      package = try
        p.package() as Package
      end
      out.print(ANSI.bold(true) + ANSI.green() + "OK" + ANSI.reset())

    | let errs: Array[Error] val =>
      out.print("Found " + ANSI.bold(true) + ANSI.red() + errs.size().string() + ANSI.reset() + " Errors:")
      for err in errs.values() do
        match err.file
        | let file: String val =>
          out.print(
            "[ " +
            file +
            ":" +
            err.position.string() +
            " ] " +
            ANSI.bold(true) +
            err.msg +
            ANSI.reset()
          )
        | None =>
          out.print(
            ANSI.bold(true) +
            err.msg +
            ANSI.reset()
          )
        end
      end
    end

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "compile",
          "Compile a pony program and spit out errors if any",
          [
            OptionSpec.string_seq("paths", "paths to add to the package search path" where short' = 'p')
          ],
          [
            ArgSpec.string("directory", "The program directory")
          ]
        )? .> add_help()?
      else
        env.exitcode(-1)  // some kind of coding error
        return
      end
    let cmd =
      match CommandParser(cs).parse(env.args, env.vars)
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
    var dir = cmd.arg("directory").string()
    if dir.size() == 0 then
      dir = "."
    end
    // extract PONYPATH
    let pony_path = PonyPath(env)
    // extract search paths from cli
    let cli_search_paths = cmd.option("paths").string_seq()

    let search_paths =
      recover val
        let tmp = Array[String val](cli_search_paths.size() + 1)
        match pony_path
        | let pp_str: String val =>
          tmp.push(pp_str)
        end
        tmp.append(cli_search_paths)
        tmp
      end

    let path = FilePath(FileAuth(env.root), dir)
    let ca = CompilerActor
    ca.compile(env.out, path, search_paths)
