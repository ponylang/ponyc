use "collections"

class CommandParser
  let _spec: CommandSpec box
  let _parent: (CommandParser box | None)

  new box create(spec': CommandSpec box) =>
    _spec = spec'
    _parent = None

  new box _sub(spec': CommandSpec box, parent': CommandParser box) =>
    _spec = spec'
    _parent = parent'

  fun box parse(
    argv: Array[String] box,
    envs: (Array[String] box | None) = None)
    : (Command | CommandHelp | SyntaxError)
  =>
    """
    Parses all of the command line tokens and env vars and returns a Command,
    or the first SyntaxError.
    """
    let tokens = argv.clone()
    try tokens.shift() end  // argv[0] is the program name, so skip it
    let options: Map[String,Option] ref = options.create()
    let args: Map[String,Arg] ref = args.create()
    _parse_command(tokens, options, args, env_map(envs), false)

  fun box _root_spec(): CommandSpec box =>
    match _parent
    | let p: CommandParser box => p._root_spec()
    else
      _spec
    end

  fun env_map(envs: (Array[String] box | None)): Map[String, String] val =>
    """
    Turns env array into a k,v map, filtering for only vars that begin with the
    command name uppercased, and then stripping that off. So:
      <COMMAND>_<KEY>=<VALUE>
    becomes:
      {key, VALUE}
    """
    let envsmap = recover Map[String, String]() end
    let prefix = _spec.name.upper() + "_"
    let prelen = prefix.size().isize()
    match envs
    | let envsarr: Array[String] box =>
      for e in envsarr.values() do
        let eqpos = try e.find("=") else 0 end // should find 1 =
        let ek: String val = e.substring(0, eqpos)
        let ev: String val = e.substring(eqpos+1)
        if ek.at(prefix, 0) then
          envsmap.update(ek.substring(prelen).lower(), ev)
        end
      end
    end
    envsmap

  fun box _parse_command(
    tokens: Array[String] ref,
    options: Map[String,Option] ref,
    args: Map[String,Arg] ref,
    envsmap: Map[String, String] box,
    ostop: Bool)
    : (Command | CommandHelp | SyntaxError)
  =>
    """
    Parses all of the command line tokens and env vars into the given options
    and args maps. Returns the first SyntaxError, or the Command when OK.
    """
    var opt_stop = ostop
    var arg_pos: USize = 0

    while tokens.size() > 0 do
      let token = try tokens.shift() else "" end
      if token == "--" then
        opt_stop = true

      elseif not opt_stop and (token.compare_sub("--", 2, 0) == Equal) then
        match _parse_long_option(token.substring(2), tokens)
        | let f: Option => options.update(f.spec.name, f)
        | let se: SyntaxError => return se
        end

      elseif not opt_stop and
        ((token.compare_sub("-", 1, 0) == Equal) and (token.size() > 1)) then
        match _parse_short_options(token.substring(1), tokens)
        | let os: Array[Option] =>
          for o in os.values() do
            options.update(o.spec.name, o)
          end
        | let se: SyntaxError =>
          return se
        end

      else // no dashes, must be a command or an arg
        if _spec.commands.contains(token) then
          try
            match _spec.commands(token)
            | let cs: CommandSpec box =>
              return CommandParser._sub(cs, this).
                _parse_command(tokens, options, args, envsmap, opt_stop)
            else
              None // TODO https://github.com/ponylang/ponyc/issues/1898
            end
          end
        else
          match _parse_arg(token, arg_pos)
          | let a: Arg => args.update(a.spec.name, a); arg_pos = arg_pos + 1
          | let se: SyntaxError => return se
          end
        end
      end
    end

    // Fill in option values from env or from coded defaults.
    for os in _spec.options.values() do
      if not options.contains(os.name) then
        // Lookup and use env vars before code defaults
        if envsmap.contains(os.name) then
          let vs =
            try
              envsmap(os.name)
            else  // TODO(cq) why else needed? we just checked
              ""
            end
          let v: Value =
            match ValueParser.parse(os.typ, vs)
            | let v: Value => v
            | let se: SyntaxError => return se
            end
          options.update(os.name, Option(os, v))
        else
          if not os.required then
            options.update(os.name, Option(os, os.default))
          end
        end
      end
    end

    // If it's a help option, return a general or specific CommandHelp.
    if options.contains(_help_name()) then
      return
        if _spec is _root_spec() then
          Help.general(_root_spec())
        else
          Help.for_command(_root_spec(), [_spec.name])
        end
    end

    // If it's a help command, return a general or specific CommandHelp.
    if _spec.name == _help_name() then
      try
        let arg = args("command").value as String
        match arg
        | "" => return Help.general(_root_spec())
        | let c: String => return Help.for_command(_root_spec(), [c])
        else
          None // TODO https://github.com/ponylang/ponyc/issues/1898
        end
      end
      return Help.general(_root_spec())
    end

    // Check for missing options and error if any exist.
    for os in _spec.options.values() do
      if not options.contains(os.name) then
        if os.required then
          return SyntaxError(os.name, "missing value for required option")
        end
      end
    end

    // Check for missing args and error if found.
    while arg_pos < _spec.args.size() do
      try
        let ars = _spec.args(arg_pos)
        if ars.required then
          return SyntaxError(ars.name, "missing value for required argument")
        end
        args.update(ars.name, Arg(ars, ars.default))
      end
      arg_pos = arg_pos + 1
    end

    // A successfully parsed and populated leaf Command
    Command(_spec, options, args)

  fun box _parse_long_option(
    token: String,
    args: Array[String] ref)
    : (Option | SyntaxError)
  =>
    """
    --opt=foo => --opt has argument foo
    --opt foo => --opt has argument foo, iff arg is required
    """
    let parts = token.split("=")
    let name = try parts(0) else "???" end
    let targ = try parts(1) else None end
    match _option_with_name(name)
    | let os: OptionSpec => OptionParser.parse(os, targ, args)
    | None => SyntaxError(name, "unknown long option")
    end

  fun box _parse_short_options(
    token: String,
    args: Array[String] ref)
    : (Array[Option] | SyntaxError)
  =>
    """
    if 'O' requires an argument
      -OFoo  => -O has argument Foo
      -O=Foo => -O has argument Foo
      -O Foo => -O has argument Foo
    else
      -O=Foo => -O has argument foo
    -abc => options a, b, c.
    -abcFoo => options a, b, c. c has argument Foo iff its arg is required.
    -abc=Foo => options a, b, c. c has argument Foo.
    -abc Foo => options a, b, c. c has argument Foo iff its arg is required.
    """
    let parts = token.split("=")
    let shorts = (try parts(0) else "" end).clone()
    var targ = try parts(1) else None end

    let options: Array[Option] ref = options.create()
    while shorts.size() > 0 do
      let c =
        try
          shorts.shift()
        else
          0  // TODO(cq) Should never error since checked
        end
      match _option_with_short(c)
      | let fs: OptionSpec =>
        if fs._requires_arg() and (shorts.size() > 0) then
          // opt needs an arg, so consume the remainder of the shorts for targ
          if targ is None then
            targ = shorts.clone()
            shorts.truncate(0)
          else
            return SyntaxError(short_string(c), "ambiguous args for short option")
          end
        end
        let arg = if shorts.size() == 0 then targ else None end
        match OptionParser.parse(fs, arg, args)
        | let f: Option => options.push(f)
        | let se: SyntaxError => return se
        end
      | None => SyntaxError(short_string(c), "unknown short option")
      end
    end
    options

  fun box _parse_arg(token: String, arg_pos: USize): (Arg | SyntaxError) =>
    try
      let arg_spec = _spec.args(arg_pos)
      ArgParser.parse(arg_spec, token)
    else
      return SyntaxError(token, "too many positional arguments")
    end

  fun box _option_with_name(name: String): (OptionSpec | None) =>
    try
      return _spec.options(name)
    end
    match _parent
    | let p: CommandParser box => p._option_with_name(name)
    else
      None
    end

  fun box _option_with_short(short: U8): (OptionSpec | None) =>
    for f in _spec.options.values() do
      if f._has_short(short) then
        return f
      end
    end
    match _parent
    | let p: CommandParser box => p._option_with_short(short)
    else
      None
    end

  fun short_string(c: U8): String =>
    recover String.from_utf32(c.u32()) end

  fun box _help_name(): String =>
    _root_spec().help_name

primitive OptionParser
  fun parse(
    spec: OptionSpec,
    targ: (String|None),
    args: Array[String] ref)
    : (Option | SyntaxError)
  =>
    // Grab the token-arg if provided, else consume an arg if one is required.
    let arg = match targ
      | (let fn: None) if spec._requires_arg() =>
        try args.shift() else None end
      else
        targ
      end
    // Now convert the arg to Type, detecting missing or mis-typed args
    match arg
    | let a: String =>
      match ValueParser.parse(spec.typ, a)
      | let v: Value => Option(spec, v)
      | let se: SyntaxError => se
      end
    else
      if not spec._requires_arg() then
        Option(spec, spec._default_arg())
      else
        SyntaxError(spec.name, "missing arg for option")
      end
    end

primitive ArgParser
  fun parse(spec: ArgSpec, arg: String): (Arg | SyntaxError) =>
    match ValueParser.parse(spec.typ, arg)
    | let v: Value => Arg(spec, v)
    | let se: SyntaxError => se
    end

primitive ValueParser
  fun box parse(typ: ValueType, arg: String): (Value | SyntaxError) =>
    try
      match typ
      | let b: BoolType => arg.bool()
      | let s: StringType => arg
      | let f: F64Type => arg.f64()
      | let i: I64Type => arg.i64()
      end
    else
      SyntaxError(arg, "unable to convert '" + arg + "' to " + typ.string())
    end
