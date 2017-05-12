
primitive Help

  fun general(cs: CommandSpec box): CommandHelp
  =>
    CommandHelp._create(None, cs)

  fun for_command(cs: CommandSpec box, argv: Array[String] box)
    : (CommandHelp | SyntaxError)
  =>
    _parse(cs, CommandHelp._create(None, cs), argv)

  fun _parse(cs: CommandSpec box, ch: CommandHelp, argv: Array[String] box)
    : (CommandHelp | SyntaxError)
  =>
    if argv.size() > 0 then
      try
        let cname = argv(0)
        if cs.commands.contains(cname) then
          match cs.commands(cname)
          | let ccs: CommandSpec box =>
            let cch = CommandHelp._create(ch, ccs)
            return _parse(ccs, cch, argv.slice(1))
          else
            // TODO https://github.com/ponylang/ponyc/issues/1898
            return SyntaxError("issue", "1898")
          end
        end
        return SyntaxError(cname, "unknown command")
      end
    end
    ch

class box CommandHelp
  """
  CommandHelp encapsulates the information needed to generate a user help
  message for a given CommandSpec, optionally with a specific command
  identified to get help on. Use `Help.general()` or `Help.for_command()` to
  create a CommandHelp instance.
  """
  let parent: (CommandHelp box | None)
  let spec: CommandSpec box

  new _create(parent': (CommandHelp box | None), spec': CommandSpec box) =>
    parent = parent'
    spec = spec'

  fun fullname(): String =>
    match parent
    | let p: CommandHelp => p.fullname() + " " + spec.name
    else
      spec.name
    end

  fun string(): String => fullname()

  fun help_string(): String =>
    let w: StringWriter ref = StringWriter
    print_help(w)
    w.string()

  fun print_help(w: Writer) =>
    print_usage(w)
    w.write("\n")
    w.write(spec.descr + "\n")
    w.write("\n")

    let options = all_options()
    if options.size() > 0 then
      w.write("Options:\n")
      print_options(w, options)
    end
    if spec.commands.size() > 0 then
      w.write("\nCommands:\n")
      print_commands(w)
    end
    let args = spec.args
    if args.size() > 0 then
      w.write("\nArgs:\n")
      print_args(w, args)
    end

  fun print_usage(w: Writer) =>
    w.write("usage: " + fullname())
    if any_options() then
      w.write(" [<options>]")
    end
    if spec.commands.size() > 0 then
      w.write(" <command>")
    end
    if spec.args.size() > 0 then
      for a in spec.args.values() do
        w.write(" " + a.help_string())
      end
    else
      w.write(" [<args> ...]")
    end
    w.write("\n")

  fun print_options(w: Writer, options: Array[OptionSpec box] box) =>
    let cols = Array[(String,String)]()
    for o in options.values() do
      cols.push(("  " + o.help_string(), o.descr))
    end
    _Columns.print(w, cols)

  fun print_commands(w: Writer) =>
    let cols = Array[(String,String)]()
    _list_commands(spec, cols, 1)
    _Columns.print(w, cols)

  fun _list_commands(
    cs: CommandSpec box,
    cols: Array[(String,String)],
    level: USize)
  =>
    for c in cs.commands.values() do
      cols.push((_Columns.indent(level*2) + c.help_string(), c.descr))
      _list_commands(c, cols, level + 1)
    end

  fun print_args(w: Writer, args: Array[ArgSpec] box) =>
    let cols = Array[(String,String)]()
    for a in args.values() do
      cols.push(("  " + a.help_string(), a.descr))
    end
    _Columns.print(w, cols)

  fun any_options(): Bool =>
    if spec.options.size() > 0 then
      true
    else
      match parent
      | let p: CommandHelp => p.any_options()
      else
        false
      end
    end

  fun all_options(): Array[OptionSpec box] =>
    let options = Array[OptionSpec box]()
    _all_options(options)
    options

  fun _all_options(options: Array[OptionSpec box]) =>
    match parent
    | let p: CommandHelp => p._all_options(options)
    end
    for o in spec.options.values() do
      options.push(o)
    end

interface Writer
  """
  This interface and two classes allow the help output to go to a general
  Writer that can be implemented for string creation or to an OutStream. Or
  other targets. This could be polished up and moved into a more central
  package.
  """
  fun ref write(data: ByteSeq)

class StringWriter
  let _s: String ref = String

  fun ref write(data: ByteSeq) =>
    _s.append(data)

  fun string(): String iso^ =>
    _s.clone()

class OutWriter
  let _os: OutStream tag

  new create(os: OutStream tag) =>
    _os = os

  fun ref write(data: ByteSeq) =>
    _os.write(data)

primitive _Columns
  fun indent(n: USize): String =>
    let spaces = "                                "
    spaces.substring(0, n.isize())

  fun print(w: Writer, cols: Array[(String,String)]) =>
    var widest: USize = 0
    for c in cols.values() do
      (let c1, _) = c
      let c1s = c1.size()
      if c1s > widest then
        widest = c1s
      end
    end
    for c in cols.values() do
      (let c1, let c2) = c
      w.write(indent(1))
      w.write(c1)
      w.write(indent((widest - c1.size()) + 2))
      w.write(c2 + "\n")
    end
