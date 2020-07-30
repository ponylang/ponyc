use "buffered"

primitive Help
  fun general(cs: CommandSpec box): CommandHelp =>
    """
    Creates a command help that can print a general program help message.
    """
    CommandHelp._create(None, cs)

  fun for_command(cs: CommandSpec box, argv: Array[String] box)
    : (CommandHelp | SyntaxError)
  =>
    """
    Creates a command help for a specific command that can print a detailed
    help message.
    """
    _parse(cs, CommandHelp._create(None, cs), argv)

  fun _parse(cs: CommandSpec box, ch: CommandHelp, argv: Array[String] box)
    : (CommandHelp | SyntaxError)
  =>
    if argv.size() > 0 then
      try
        let cname = argv(0)?
        if cs.commands().contains(cname) then
          match cs.commands()(cname)?
          | let ccs: CommandSpec box =>
            let cch = CommandHelp._create(ch, ccs)
            return _parse(ccs, cch, argv.slice(1))
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
  identified to print help about. Use `Help.general()` or `Help.for_command()`
  to create a CommandHelp instance.
  """
  let _parent: (CommandHelp box | None)
  let _spec: CommandSpec box

  new _create(parent': (CommandHelp box | None), spec': CommandSpec box) =>
    _parent = parent'
    _spec = spec'

  fun box fullname(): String =>
    match _parent
    | let p: CommandHelp => p.fullname() + " " + _spec.name()
    else
      _spec.name()
    end

  fun box string(): String => fullname()

  fun box help_string(): String =>
    """
    Renders the help message as a String.
    """
    let w: Writer = Writer
    _write_help(w)
    let str = recover trn String(w.size()) end
    for bytes in w.done().values() do str.append(String.from_array(bytes)) end
    str

  fun box print_help(os: OutStream) =>
    """
    Prints the help message to an OutStream.
    """
    let w: Writer = Writer
    _write_help(w)
    os.writev(w.done())

  fun box _write_help(w: Writer) =>
    _write_usage(w)
    if _spec.descr().size() > 0 then
      w.write("\n")
      w.write(_spec.descr() + "\n")
    end

    let options = _all_options()
    if options.size() > 0 then
      w.write("\nOptions:\n")
      _write_options(w, options)
    end
    if _spec.commands().size() > 0 then
      w.write("\nCommands:\n")
      _write_commands(w)
    end
    let args = _spec.args()
    if args.size() > 0 then
      w.write("\nArgs:\n")
      _write_args(w, args)
    end

  fun box _write_usage(w: Writer) =>
    w.write("usage: " + fullname())
    if _any_options() then
      w.write(" [<options>]")
    end
    if _spec.commands().size() > 0 then
      w.write(" <command>")
    end
    if _spec.args().size() > 0 then
      for a in _spec.args().values() do
        w.write(" " + a.help_string())
      end
    else
      w.write(" [<args> ...]")
    end
    w.write("\n")

  fun box _write_options(w: Writer, options: Array[OptionSpec box] box) =>
    let cols = Array[(USize,String,String)]()
    for o in options.values() do
      cols.push((2, o.help_string(), o.descr()))
    end
    _Columns.write(w, cols)

  fun box _write_commands(w: Writer) =>
    let cols = Array[(USize,String,String)]()
    _list_commands(_spec, cols, 1)
    _Columns.write(w, cols)

  fun box _list_commands(
    cs: CommandSpec box,
    cols: Array[(USize,String,String)],
    level: USize)
  =>
    for c in cs.commands().values() do
      cols.push((level*2, c.help_string(), c.descr()))
      _list_commands(c, cols, level + 1)
    end

  fun box _write_args(w: Writer, args: Array[ArgSpec] box) =>
    let cols = Array[(USize,String,String)]()
    for a in args.values() do
      cols.push((2, a.help_string(), a.descr()))
    end
    _Columns.write(w, cols)

  fun box _any_options(): Bool =>
    if _spec.options().size() > 0 then
      true
    else
      match _parent
      | let p: CommandHelp => p._any_options()
      else
        false
      end
    end

  fun box _all_options(): Array[OptionSpec box] =>
    let options = Array[OptionSpec box]()
    _all_options_fill(options)
    options

  fun box _all_options_fill(options: Array[OptionSpec box]) =>
    match _parent
    | let p: CommandHelp => p._all_options_fill(options)
    end
    for o in _spec.options().values() do
      options.push(o)
    end

primitive _Columns
  fun indent(w: Writer, n: USize) =>
    var i = n
    while i > 0 do
      w.write(" ")
      i = i - 1
    end

  fun write(w: Writer, cols: Array[(USize,String,String)]) =>
    var widest: USize = 0
    for c in cols.values() do
      (let c0, let c1, _) = c
      let c1s = c0 + c1.size()
      if c1s > widest then
        widest = c1s
      end
    end
    for c in cols.values() do
      (let c0, let c1, let c2) = c
      indent(w, 1 + c0)
      w.write(c1)
      indent(w, (widest - c1.size()) + 2)
      w.write(c2 + "\n")
    end
