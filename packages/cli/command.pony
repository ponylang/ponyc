use "collections"

class box Command
  """
  Command contains all of the information describing a command with its spec
  and effective options and arguments, ready to use.
  """
  let _spec: CommandSpec box
  let _fullname: String
  let _options: Map[String, Option] box
  let _args: Map[String, Arg] box

  new _create(
    spec': CommandSpec box,
    fullname': String,
    options': Map[String, Option] box,
    args': Map[String, Arg] box)
  =>
    _spec = spec'
    _fullname = fullname'
    _options = options'
    _args = args'

  fun string(): String iso^ =>
    """
    Returns a representational string for this command.
    """
    let s: String iso = fullname().clone()
    for o in _options.values() do
      s.append(" ")
      s.append(o.deb_string())
    end
    for a in _args.values() do
      s.append(" ")
      s.append(a.deb_string())
    end
    s

  fun spec() : CommandSpec box =>
    """
    Returns the spec for this command.
    """
    _spec

  fun fullname() : String =>
    """
    Returns the full name of this command, with its parents prefixed.
    """
    _fullname

  fun box option(name: String): Option =>
    """
    Returns the Option by name, defaulting to a fake Option if unknown.
    """
    try _options(name)? else Option(OptionSpec.bool(name), false) end

  fun box arg(name: String): Arg =>
    """
    Returns the Arg by name, defaulting to a fake Arg if unknown.
    """
    try _args(name)? else Arg(ArgSpec.bool(name), false) end

class val Option
  """
  Option contains a spec and an effective value for a given option.
  """
  let _spec: OptionSpec
  let _value: _Value

  new val create(spec': OptionSpec, value': _Value) =>
    _spec = spec'
    _value = value'

  fun _append(next: Option): Option =>
    Option(_spec, _spec._typ_p().append(_value, next._value))

  fun spec() : OptionSpec => _spec

  fun bool(): Bool =>
    """
    Returns the option value as a Bool, defaulting to false.
    """
    try _value as Bool else false end

  fun string(): String =>
    """
    Returns the option value as a String, defaulting to empty.
    """
    try _value as String else "" end

  fun i64(): I64 =>
    """
    Returns the option value as an I64, defaulting to 0.
    """
    try _value as I64 else I64(0) end

  fun f64(): F64 =>
    """
    Returns the option value as an F64, defaulting to 0.0.
    """
    try _value as F64 else F64(0) end

  fun string_seq(): ReadSeq[String] val =>
    """
    Returns the option value as a ReadSeq[String], defaulting to empty.
    """
    try
      _value as _StringSeq val
    else
      recover val Array[String]() end
    end

  fun deb_string(): String =>
    _spec.deb_string() + "=" + _value.string()

class val Arg
  """
  Arg contains a spec and an effective value for a given arg.
  """
  let _spec: ArgSpec
  let _value: _Value

  new val create(spec': ArgSpec, value': _Value) =>
    _spec = spec'
    _value = value'

  fun _append(next: Arg): Arg =>
    Arg(_spec, _spec._typ_p().append(_value, next._value))

  fun spec(): ArgSpec => _spec

  fun bool(): Bool =>
    """
    Returns the arg value as a Bool, defaulting to false.
    """
    try _value as Bool else false end

  fun string(): String =>
    """
    Returns the arg value as a String, defaulting to empty.
    """
    try _value as String else "" end

  fun i64(): I64 =>
    """
    Returns the arg value as an I64, defaulting to 0.
    """
    try _value as I64 else I64(0) end

  fun f64(): F64 =>
    """
    Returns the arg value as an F64, defaulting to 0.0.
    """
    try _value as F64 else F64(0) end

  fun string_seq(): ReadSeq[String] val =>
    """
    Returns the arg value as a ReadSeq[String], defaulting to empty.
    """
    try
      _value as _StringSeq val
    else
      recover val Array[String]() end
    end

  fun deb_string(): String =>
    "(" + _spec.deb_string() + "=)" + _value.string()

class val SyntaxError
  """
  SyntaxError summarizes a syntax error in a given parsed command line.
  """
  let _token: String
  let _msg: String

  new val create(token': String, msg': String) =>
    _token = token'
    _msg = msg'

  fun token(): String => _token

  fun string(): String => "Error: " + _msg + " at: '" + _token + "'"
