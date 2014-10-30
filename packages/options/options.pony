primitive StringArgument is Comparable[StringArgument]
primitive I64Argument is Comparable[I64Argument]
primitive F64Argument is Comparable[F64Argument]
primitive ParseError is Comparable[ParseError]

primitive _Ambiguous is Comparable[_Ambiguous]

type _Match is (None | Option | _Ambiguous)
type _ArgType is (None | StringArgument | I64Argument | F64Argument)
type _ParsedOption is ((String | None), (None | String | I64 | F64))
type _Result is (_ParsedOption | ParseError)

class Option is Stringable
  var name: String
  var short: (String | None)
  var arg: _ArgType
  var _help: (String | None)
  var _domain: Array[(String, (String | None))]

  new create(name': String, short': (String | None), help: (String | None),
    arg': _ArgType)
    =>
    name = name'
    short = short'
    arg = arg'
    _help = help
    _domain = Array[(String, (String | None))]

  fun ref has_domain(): Bool =>
    _domain.length() > 0

  fun ref add_param(value: String, help: (String | None)) =>
    _domain.append((value, help))

  fun ref accepts(value: String val): Bool =>
    try
      for (v, h) in _domain.values() do
        if v == value then return true end
      end
    end

    false

  fun ref requires_arg(): Bool =>
    match arg
    | None => return false
    end
    true

  fun box token(): String =>
    match (name, short)
    | (String, var s: String) => return "(" + name + ", " + s + ")"
    end

    "(" + name + ")"

  //TODO: Requires proper string formatter
  fun box string(): String =>
    ""

class Options ref is Iterator[_Result]
  var _env: Env
  var _args: Array[String ref]
  var _configuration: Array[(String | Option)]
  var _index: U64
  var _error: Bool

  new create(env: Env) =>
    _env = env
    _args = Array[String ref].prealloc(_env.args.length())

    try
      for i in _env.args.values() do
        _args.append(i.clone())
      end
    end

    _configuration = Array[(String | Option)]
    _index = 0
    _error = false

  fun ref remaining(): Array[String ref] => _args

  fun ref usage_text(s: String): Options =>
    _configuration.append(s)
    this

  fun ref add(name: String, short: (String | None), help: (String | None),
    arg: _ArgType): Options
    =>
    _configuration.append(Option(name, short, help, arg))
    this

  fun ref param(value: String, help: (String | None)): Options =>
    for i in Reverse[U64](_configuration.length(), 0) do
      try
        match _configuration(i)
        | var option: Option =>
          option.add_param(value, help)
          return this
        end
      else
        return this
      end
    end

    _error = true

    _env.stdout.print("[Options Error]: Attempt to attach parameter \""
        + value + "\" without an option")

    this

  fun box usage() =>
    var help: String iso = recover String end

    try
      for i in _configuration.values() do
        var s: Stringable = i
        help.append(s.string())
      end

      _env.stdout.print(consume help)
    end

  fun ref _skip_non_options(): Bool =>
    while _index < _args.length() do
      try
        let current = _args(_index)

        if (current(0) == "-".u8()) and (current(1) != 0) then
          return true
        end
      end

      _index = _index + 1
    end

    false

  fun ref _select(s: String) : ((Option | None), (Option | None)) =>
    var long: (Option | None) = None
    var short: (Option | None) = None

    try
      for i in _configuration.values() do
        match i
        | var opt: Option =>
          let ln = opt.name

          if
            (ln.compare(s, s.length()) == 0) and (ln.length() == s.length())
          then
            long = opt
          end

          match opt.short
          | var sn : String =>
            if sn.compare(s, 1) == 0 then
              short = opt
            end
          end
        end
      end
    else
      return (None, None)
    end

    (long, short)

  fun ref strip_accepted() =>
    try
      let current = _args(_index)
      let dangling_short = (current.length() == 1) and (current(0) == "-".u8())
      let dangling_long = current.length() == 0

      if dangling_short or dangling_long then
        _args.delete(_index)
        _index = _index + 1
      end
    end

  fun ref _find_match(): _Match =>
    try
      let current = _args(_index)

      let start: I64 =
        match (current(0), current(1))
        | ("-".u8(), "-".u8()) => I64(2)      //TODO: remove when literal
        | ("-".u8(), var some: Any) => I64(1) //inference works
        else
          error //cannot happen, otherwise current would have been identified by
                //_skip_non_options
        end

      let finish = current.find("=")
      let name: String val = current.substring(start, finish)

      match _select(name)
      | (var x: Option, var y: Option) =>
        if x isnt y then
          _env.stdout.print("[Options Error]: The two options " +
            x.token() + " and " y.token() + " are ambiguous!")
          _Ambiguous
        end
      | (None, var y: Option) =>
        _args(_index) = current.cut_in_place(1, 1) ; strip_accepted() ; y
      | (var x: Option, None) =>
        _args(_index) = current.cut_in_place(0, finish) ; strip_accepted() ; x
      else
        error
      end
    else
      None
    end

  fun box has_next(): Bool =>
    not _error and (_index < _args.length())

  fun ref next(): _Result =>
    if _skip_non_options() then
      match _find_match()
      | _Ambiguous => _error = true ; ParseError
      | None =>
        _index = _index + 1
        if has_next() then
          return next()
        else
          return (None, None)
        end
      | var m: Option =>
        if m.requires_arg() then
          try
            let argument: String val = _args(_index).clone()

            if m.has_domain() and not m.accepts(argument) then error end

            match m.arg
            | StringArgument => return (m.name, argument)
            | I64Argument => return (m.name, argument.i64())
            | F64Argument => return (m.name, argument.f64())
            end

            try _args.delete(_index) end
            _index = _index + 1
          else
            _error = true
            _env.stdout.print("[Options Error]:" +
              " No argument supplied or the supplied argument is not in the" +
              " option's domain: --" + m.name)
            return ParseError
          end
        end
        return (m.name, None)
      end
    end

    (None, None)
