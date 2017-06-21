use "collections"

class box Command
  """
  Command contains all of the information describing a command with its spec
  and effective options and arguments, ready to use.
  """
  let _spec: CommandSpec box
  let _options: Map[String, Option] box
  let _args: Map[String, Arg] box

  new create(
    spec': CommandSpec box,
    options': Map[String, Option] box,
    args': Map[String, Arg] box)
  =>
    _spec = spec'
    _options = options'
    _args = args'

  fun string(): String =>
    let s: String iso = _spec.name().clone()
    for o in _options.values() do
      s.append(" ")
      s.append(o.deb_string())
    end
    for a in _args.values() do
      s.append(" ")
      s.append(a.deb_string())
    end
    s

  fun box spec() : CommandSpec box => _spec

  fun box option(name: String): Option =>
    try _options(name) else Option(OptionSpec.bool(name), false) end

  fun box arg(name: String): Arg =>
    try _args(name) else Arg(ArgSpec.bool(name), false) end

class val Option
  """
  Option contains a spec and an effective value for a given option.
  """
  let _spec: OptionSpec
  let _value: _Value

  new val create(spec': OptionSpec, value': _Value) =>
    _spec = spec'
    _value = value'

  fun spec() : OptionSpec => _spec

  fun bool(): Bool =>
    try _value as Bool else false end

  fun string(): String =>
    try _value as String else "" end

  fun i64(): I64 =>
    try _value as I64 else I64(0) end

  fun f64(): F64 =>
    try _value as F64 else F64(0) end

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

  fun spec(): ArgSpec => _spec

  fun bool(): Bool =>
    try _value as Bool else false end

  fun string(): String =>
    try _value as String else "" end

  fun i64(): I64 =>
    try _value as I64 else I64(0) end

  fun f64(): F64 =>
    try _value as F64 else F64(0) end

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
