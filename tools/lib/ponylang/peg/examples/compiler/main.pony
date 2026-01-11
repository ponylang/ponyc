use "files"
use "../../peg"

actor Main
  new create(env: Env) =>
    try
      match env.args.size()
      | 2 =>
        let auth = FileAuth(env.root)
        let filename = env.args(1)?
        let p = recover val (JsonParser() / PegParser()).eof() end
        peg_run(p, filename, auth, env.out)
      | 3 =>
        peg_compiler(env)
      else
        let cmd_name = try env.args(0)? else "compiler" end
        env.out.print("\n".join(
          [ "Usage:"
            "  " + cmd_name + " <source-file>"
            "      Run a PEG/JSON parser over some source file and print the AST."
            "  " + cmd_name + " <peg-file> <source-file>"
            "      Compile a parser from the first file, then run it over the "
              + "second file and"
            "      print the AST."
          ].values()))
        env.exitcode(1)
      end
    end

  fun peg_run(p: Parser val,
    filename: String,
    auth: FileAuth,
    out: OutStream)
  =>
    """
    Run a parser over some source file and print the AST.
    """
    try
      let source = Source(FilePath(auth, filename))?
      match recover val p.parse(source) end
      | (_, let r: ASTChild) =>
        out.print(recover val Printer(r) end)
      | (let offset: USize, let r: Parser val) =>
        let e = recover val SyntaxError(source, offset, r) end
        out.writev(PegFormatError.console(e))
      end
    else
      out.print("Couldn't open file " + filename)
    end

  fun peg_compiler(env: Env) =>
    """
    Compile a parser from the first file, then run it over the second file and
    print the AST.
    """
    try
      let peg_filename = env.args(1)?
      let target_filename = env.args(2)?
      let auth = FileAuth(env.root)
      let peg = Source(FilePath(auth, peg_filename))?

      match recover val PegCompiler(peg) end
      | let p: Parser val =>
        peg_run(p, target_filename, auth, env.out)
      | let errors: Array[PegError] val =>
        for e in errors.values() do
          env.out.writev(PegFormatError.console(e))
        end
      end
    end
