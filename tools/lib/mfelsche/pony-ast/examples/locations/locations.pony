use "../../ast"
use "cli"
use "debug"
use "files"
use "term"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "locations",
          "Debug the reported locations for each AST node",
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
    match Compiler.compile(path, search_paths)
    | let program: Program =>
      try
        (program.package() as Package val).ast.visit(_ASTLocationVisitor(env.out))
      else
        env.err.print("No package in our program???")
      end
    | let errors: Array[Error] val =>
      env.out.print("Found " + ANSI.bold(true) + ANSI.red() + errors.size().string() + ANSI.reset() + " Errors:")
      for err in errors.values() do
        match err.file
        | let file: String val =>
          env.out.print(
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
          env.out.print(
            ANSI.bold(true) +
            err.msg +
            ANSI.reset()
          )
        end
      end
    end

class _ASTLocationVisitor is ASTVisitor
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref visit(ast: AST box): VisitResult =>
    try
      var num_parents: USize = 0
      var parent = ast.parent()
      while parent isnt None do
        num_parents = num_parents + 1
        parent = (parent as AST box).parent()
        _out.write(" ")
      end
      _out.print(ast.debug())
      Continue
    else
      Stop
    end
