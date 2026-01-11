use "files"
use "cli"
use "../../ast"
use "debug"

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf(
          "find_type",
          "Find the type at a certain position in your pony program",

          [
            OptionSpec.string_seq("paths", "paths to add to the package search path" where short' = 'p')
          ],
          [
            ArgSpec.string("directory", "The program directory")
            ArgSpec.string("file", "The file to search for a type in")
            ArgSpec.u64("line", "The line in the source file, starts with 1")
            ArgSpec.u64("column", "The column on the given line, starts with 1")
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
    try
      let program_dir_str = cmd.arg("directory").string()
      if program_dir_str.size() == 0 then
        env.err.print("Missing or invalid directory")
        error
      end
      let program_dir = FilePath.create(FileAuth(env.root), program_dir_str)
      let file = cmd.arg("file").string()
      if file.size() == 0 then
        env.err.print("Missing or invalid file")
        error
      end
      let line = cmd.arg("line").u64()
      if line == 0 then
        env.err.print("Missing or invalid line")
        error
      end
      let column = cmd.arg("column").u64()
      if column == 0 then
        env.err.print("Missing or invalid column")
        error
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
      find_type(env, program_dir, search_paths, file, line.usize(), column.usize())
    else
      env.exitcode(1)
    end

  be find_type(
    env: Env,
    program_dir: FilePath,
    search_paths: ReadSeq[String val] val,
    file: String,
    line: USize,
    column: USize)
  =>
    try
      match Compiler.compile(program_dir, search_paths)
      | let program: Program =>
        env.out.print("OK")
        let t = get_type_at(file, line, column, program.package() as Package) as String
        env.out.print("Type: " + t)
      | let errors: Array[Error] val =>
        env.out.print("ERROR")
      end
    else
      env.exitcode(1)
    end

  fun get_type_at(file: String, line: USize, column: USize, package: Package): (String | None) =>
    try
      let module = package.find_module(file) as Module

      let index = module.create_position_index()

      match index.find_node_at(line, column)
      | let ast: AST box =>
        Debug("FOUND " + TokenIds.string(ast.id()))
        Types.get_ast_type(ast)
      | None => None
      end
    end

