"""
This package implements command line parsing with the notion of commands that
are specified as a hierarchy.
See RFC-xxx for more details.

The general EBNF of the command line looks like:
  command_line ::= root_command (option | command)* (option | arg)*
  command ::= alphanum_word
  alphanum_word ::= alphachar(alphachar | numchar | '_' | '-')*
  option ::= longoption | shortoptionset
  longoption ::= '--'alphanum_word['='arg | ' 'arg]
  shortoptionset := '-'alphachar[alphachar]...['='arg | ' 'arg]
  arg := boolarg | intarg | floatarg | stringarg
  boolarg := 'true' | 'false'
  intarg> := ['-'] numchar...
  floatarg ::= ['-'] numchar... ['.' numchar...]
  stringarg ::= anychar

Some Examples:
  usage: chat [<options>] <command> [<options>] [<args> ...]
"""
use "collections"

class CommandSpec
  """
  CommandSpec describes the specification of a parent or leaf command. Each
  command has the following attributes:

  - a name: a simple string token that identifies the command.
  - a description: used in the syntax message.
  - a map of options: the valid options for this command.
  - an optional help option+command name for help parsing
  - one of:
     - a Map of child commands.
     - an Array of arguments.
  """
  let name: String
  let descr: String
  let options: Map[String, OptionSpec] = options.create()
  var help_name: String = ""

  // A parent commands can have sub-commands; leaf commands can have args.
  let commands: Map[String, CommandSpec box] = commands.create()
  let args: Array[ArgSpec] = args.create()

  new parent(
    name': String,
    descr': String = "",
    options': Array[OptionSpec] box = Array[OptionSpec](),
    commands': Array[CommandSpec] box = Array[CommandSpec]()) ?
  =>
    """
    Create a command spec that can accept options and child commands, but not
    arguments.
    """
    name = _assertName(name')
    descr = descr'
    for o in options'.values() do
      options.update(o.name, o)
    end
    for c in commands'.values() do
      commands.update(c.name, c)
    end

  new leaf(
    name': String,
    descr': String = "",
    options': Array[OptionSpec] box = Array[OptionSpec](),
    args': Array[ArgSpec] box = Array[ArgSpec]()) ?
  =>
    """
    Create a command spec that can accept options and arguments, but not child
    commands.
    """
    name = _assertName(name')
    descr = descr'
    for f in options'.values() do
      options.update(f.name, f)
    end
    for a in args'.values() do
      args.push(a)
    end

  fun tag _assertName(nm: String): String ? =>
    for b in nm.values() do
      if (b != '-') and (b != '_') and
        not ((b >= '0') and (b <= '9')) and
        not ((b >= 'A') and (b <= 'Z')) and
        not ((b >= 'a') and (b <= 'z')) then
        error
      end
    end
    nm

  fun ref add_command(cmd: CommandSpec) ? =>
    """
    Add an additional child command to this parent command.
    """
    if args.size() > 0 then error end
    commands.update(cmd.name, cmd)

  fun ref add_help(hname: String = "help") ? =>
    """
    Add a standard help option and command to this root command.
    """
    if args.size() > 0 then error end
    help_name = hname
    let help_cmd = CommandSpec.leaf(help_name, "", Array[OptionSpec](), [
      ArgSpec.string("command" where default' = "")
    ])
    commands.update(help_cmd.name, help_cmd)
    let help_option = OptionSpec.bool(help_name, "", 'h', false)
    options.update(help_option.name, help_option)

  fun help_string(): String =>
    let s = name.clone()
    s.append(" ")
    for a in args.values() do
      s.append(a.help_string())
    end
    s

class val OptionSpec
  """
  OptionSpec describes the specification of an option. Options have a name,
  descr(iption), a short-name, a typ(e), and a default value when they are not
  required. Options can be placed anywhere before or after commands, and can be
  thought of like named arguments.
  """
  let name: String
  let descr: String
  let short: (U8 | None)
  let typ: ValueType
  let default: Value
  let required: Bool

  fun tag _init(typ': ValueType, default': (Value | None))
    : (ValueType, Value, Bool) ?
  =>
    match default'
    | None =>
      (typ', false, true)
    else
      (typ', default' as Value, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (Bool | None) = None) ?
  =>
    name = name'
    descr = descr'
    short = short'
    (typ, default, required) = _init(BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (String | None) = None) ?
  =>
    name = name'
    descr = descr'
    short = short'
    (typ, default, required) = _init(StringType, default')

  new val i64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (I64 | None) = None) ?
  =>
    name = name'
    descr = descr'
    short = short'
    (typ, default, required) = _init(I64Type, default')

  new val f64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (F64 | None) = None) ?
  =>
    name = name'
    descr = descr'
    short = short'
    (typ, default, required) = _init(F64Type, default')

  // Other than bools, all options require args.
  fun _requires_arg(): Bool =>
    match typ
    | let _: BoolType => false
    else
      true
    end

  // Used for bool options to get the true arg when option is present w/o arg
  fun _default_arg(): Value =>
    match typ
    | let _: BoolType => true
    else
      false
    end

  fun _has_short(sh: U8): Bool =>
    match short
    | let ss: U8 => sh == ss
    else
      false
    end

  fun deb_string(): String =>
    "--" + name + "[" + typ.string() + "]" +
      if not required then "(=" + default.string() + ")" else "" end

  fun help_string(): String =>
    let s =
      match short
      | let ss: U8 => "-" + String.from_utf32(ss.u32()) + ", "
      else
        "    "
      end
    s + "--" + name

class val ArgSpec
  """
  ArgSpec describes the specification of a positional argument. Args always
  come after a leaf command, and are assigned in their positional order.
  """
  let name: String
  let descr: String
  let typ: ValueType
  let default: Value
  let required: Bool

  fun tag _init(typ': ValueType, default': (Value | None))
    : (ValueType, Value, Bool) ?
  =>
    match default'
    | None =>
      (typ', false, true)
    else
      (typ', default' as Value, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    default': (Bool|None) = None) ?
  =>
    name = name'
    descr = descr'
    (typ, default, required) = _init(BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    default': (String|None) = None) ?
  =>
    name = name'
    descr = descr'
    (typ, default, required) = _init(StringType, default')

  new val i64(
    name': String,
    descr': String = "",
    default': (I64|None) = None) ?
  =>
    name = name'
    descr = descr'
    (typ, default, required) = _init(I64Type, default')

  new val f64(
    name': String,
    descr': String = "",
    default': (F64|None) = None) ?
  =>
    name = name'
    descr = descr'
    (typ, default, required) = _init(F64Type, default')

  fun deb_string(): String =>
    name + "[" + typ.string() + "]" +
      if not required then "(=" + default.string() + ")" else "" end

  fun help_string(): String =>
    "<" + name + ">"

type Value is (Bool | String | I64 | F64)

primitive BoolType
  fun string(): String => "Bool"

primitive StringType
  fun string(): String => "String"

primitive I64Type
  fun string(): String => "I64"

primitive F64Type
  fun string(): String => "F64"

type ValueType is
  ( BoolType
  | StringType
  | I64Type
  | F64Type )
