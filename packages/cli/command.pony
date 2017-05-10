use col = "collections"

class box Command
  """
  Command contains all of the information describing a command with its spec
  and effective options and arguments, ready to use.
  """
  let spec: CommandSpec box
  let options: col.Map[String, Option]
  let args: col.Map[String, Arg]

  new create(spec': CommandSpec box, options': col.Map[String, Option],
      args': col.Map[String, Arg])
    =>
      spec = spec'
      options = options'
      args = args'

  fun string(): String =>
    let s: String iso = spec.name.clone()
    for o in options.values() do
      s.append(" ")
      s.append(o.string())
    end
    for a in args.values() do
      s.append(" ")
      s.append(a.string())
    end
    s


class val Option
  """
  Option contains a spec and an effective value for a given option.
  """
  let spec: OptionSpec
  let value: Value

  new val create(spec': OptionSpec, value': Value) =>
    spec = spec'
    value = value'

  fun string(): String =>
    spec.deb_string() + "=" + value.string()


class val Arg
  """
  Arg contains a spec and an effective value for a given arg.
  """
  let spec: ArgSpec
  let value: Value

  new val create(spec': ArgSpec, value': Value) =>
    spec = spec'
    value = value'

  fun string(): String =>
    "(" + spec.deb_string() + "=)" + value.string()


class val SyntaxError
  """
  SyntaxError summarizes a syntax error in a given parsed command line.
  """
  let token: String
  let msg: String

  new val create(token': String, msg': String) =>
    token = token'
    msg = msg'

  fun string(): String =>
    "Error: " + msg + " at: '" + token + "'"
