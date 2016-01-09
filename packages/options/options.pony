"""
## PonyOptions package
"""

primitive StringArgument
primitive I64Argument
primitive F64Argument
primitive Required
primitive Optional

primitive UnrecognisedOption
primitive AmbiguousMatch
primitive MissingArgument
primitive InvalidArgument

type ArgumentType is
  ( None
  | StringArgument
  | I64Argument
  | F64Argument)

type ErrorReason is
  ( UnrecognisedOption
  | MissingArgument
  | InvalidArgument
  | AmbiguousMatch)

type ParsedOption is (String, (None | String | I64 | F64))

interface ParseError
  fun reason(): ErrorReason
  fun report(out: OutStream)

class Options is Iterator[(ParsedOption | ParseError | None)]
  let _arguments: Array[String ref]
  let _fatal: Bool
  var _configuration: Array[_Option] = _configuration.create()
  var _index: USize = 0
  var _error: Bool = false

  new create(env: Env, fatal: Bool = true) =>
    _arguments = _arguments.create(env.args.size())
    _fatal = fatal

    for arg in env.args.values() do
      _arguments.push(arg.clone())
    end

  fun ref add(long: String, short: (None | String) = None,
    arg: ArgumentType = None, mode: (Required | Optional) = Required): Options
  =>
    """
    Adds a new named option to the parser configuration.
    """
    _configuration.push(_Option(long, short, arg, mode))
    this

  fun ref remaining(): Array[String ref] =>
    """
    Returns all unprocessed command line arguments. After parsing all options,
    this will only include positional arguments, potentially unrecognised and
    ambiguous options and invalid arguments.
    """
    _arguments

  fun ref _strip(opt: _Option, matched: String ref,
    start: ISize, finish: ISize)
  =>
    """
    Strips accepted options from the copied array of command line arguments.
    """
    matched.cut_in_place(start, finish)

    if opt.has_argument() then
      // If 'matched' is non-empty, then the rest (without - or =) must be the
      // argument.
      matched.lstrip("-").remove("=")
    end

    try
      if matched.size() == 0 then
        _arguments.delete(_index)
      end
    end

  fun ref _select(candidate: String ref, start: ISize, offset: ISize,
    finish: ISize): (_Option | ParseError)
  =>
    """
    Selects an option from the configuration depending on the current command
    line argument.
    """
    let name: String box = candidate.substring(start, finish)
    let matches = Array[_Option]
    var selected: (_Option | None) = None

    for opt in _configuration.values() do
      if opt.matches(name, start == 1) then
        matches.push(opt)
        selected = opt
      end
    end

    match (selected, matches.size())
    | (let opt: _Option, 1) => _strip(opt, candidate, offset, finish) ; opt
    | (let opt: _Option, _) => _ErrorPrinter._ambiguous(matches)
    else
      _ErrorPrinter._unrecognised(candidate.substring(0, finish + 1))
    end

  fun ref _skip(): Bool =>
    """
    Skips all non-options. Returns true if a named option has been found, false
    otherwise.
    """
    while _index < _arguments.size() do
      try
        let current = _arguments(_index)

        if (current(0) == '-') and (current(1) != 0) then
          return true
        end
      end

      _index = _index + 1
    end

    false

  fun ref _verify(opt: _Option, combined: Bool): (ParsedOption | ParseError) =>
    """
    Verifies whether a parsed option from the command line is well-formed. That
    is, checking whether required or optional arguments are supplied. Returns
    a ParsedOption on success, a ParseError otherwise.
    """
    if opt.has_argument() then
      try
        let argument = _arguments(_index)
        let invalid = not combined and (argument(0) == '-')

        if not opt.accepts(argument) or invalid then
          if opt.mode isnt Optional then
            return _ErrorPrinter._invalid(argument, opt)
          end
          error
        end

        _arguments.delete(_index)

        match opt.arg
        | StringArgument => return (opt.long, argument.clone())
        | I64Argument => return (opt.long, argument.i64())
        | F64Argument => return (opt.long, argument.f64())
        end
      else
        if opt.requires_argument() then
          return _ErrorPrinter._missing(opt)
        end
      end
    end

    (opt.long, None)

  fun has_next(): Bool =>
    """
    Parsing options is done if either an error occurs and fatal error reporting
    is turned on, or if all command line arguments have been processed.
    """
    not (_error and _fatal) and (_index < _arguments.size())

  fun ref next(): (ParsedOption | ParseError | None) =>
    """
    Skips all positional arguments and attemps to match named options. Returns
    a ParsedOption on success, a ParseError on error, or None if no named
    options are found.
    """
    if _skip() then
      try
        let candidate = _arguments(_index)

        (let start: ISize, let offset: ISize) =
          match (candidate(0), candidate(1))
          | ('-', '-') => (2, 0)
          | ('-', var char: U8) => (1, 1)
          else
            (0, 0) // unreachable
          end

        let last = candidate.size().isize()
        (let finish: ISize, let combined: Bool) =
          try
            (candidate.find("="), true)
          else
            (if start == 1 then start+1 else last end, false)
          end

        match _select(candidate, start, offset, finish)
        | let err: ParseError => _error = true ; _index = _index + 1 ; err
        | let opt: _Option => _verify(opt, combined)
        end
      end
    end

//TODO: Refactor
class _Option
  let long: String
  let short: (String | None)
  let arg: ArgumentType
  let mode: (Required | Optional)

  new create(long': String, short': (String | None), arg': ArgumentType,
    mode': (Required | Optional))
  =>
    long = long'
    short = short'
    arg = arg'
    mode = mode'

  fun matches(name: String box, shortmatch: Bool): Bool =>
    match (short, shortmatch)
    | (var x: String, true) => return name.compare_sub(x, 1) is Equal
    end

    long == name

  fun has_argument(): Bool =>
    match arg
    | None => return false
    end
    true

  fun requires_argument(): Bool =>
    if arg isnt None then
      match mode
      | Required => return true
      end
    end
    false

  fun accepts(argument: String box): Bool =>
    true

class _ErrorPrinter
  var _message: String
  var _reason: ErrorReason

  new _ambiguous(matches: Array[_Option]) =>
    let m = recover String end
    m.append("Ambiguous options:\n")

    for opt in matches.values() do
      m.append("  --" + opt.long)
      try m.append(", -" + (opt.short as String)) end
      m.append("\n")
    end

    _message = consume m
    _reason = AmbiguousMatch

  new _unrecognised(option: String box) =>
    _message = "Unrecognised option: " + option
    _reason = UnrecognisedOption

  new _invalid(argument: String box, option: _Option) =>
    _message = "Invalid argument: --" + option.long
    _reason = InvalidArgument

  new _missing(option: _Option) =>
    _message = "Missing argument: --" + option.long
    _reason = MissingArgument

  fun reason(): ErrorReason =>
    _reason

  fun report(out: OutStream) =>
    out.print(_message)
