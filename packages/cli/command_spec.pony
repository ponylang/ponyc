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
  let _name: String
  let _descr: String
  let _options: Map[String, OptionSpec] = _options.create()
  var _help_name: String = ""

  // A parent commands can have sub-commands; leaf commands can have args.
  let _commands: Map[String, CommandSpec box] = _commands.create()
  let _args: Array[ArgSpec] = _args.create()

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
    _name = _assertName(name')
    _descr = descr'
    for o in options'.values() do
      _options.update(o.name(), o)
    end
    for c in commands'.values() do
      _commands.update(c.name(), c)
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
    _name = _assertName(name')
    _descr = descr'
    for o in options'.values() do
      _options.update(o.name(), o)
    end
    for a in args'.values() do
      _args.push(a)
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

  fun ref add_command(cmd: CommandSpec box) ? =>
    """
    Add an additional child command to this parent command.
    """
    if _args.size() > 0 then error end
    _commands.update(cmd.name(), cmd)

  fun ref add_help(hname: String = "help") ? =>
    """
    Add a standard help option and, optionally, command to a root command.
    """
    _help_name = hname
    let help_option = OptionSpec.bool(_help_name, "", 'h', false)
    _options.update(_help_name, help_option)
    if _args.size() == 0 then
      let help_cmd = CommandSpec.leaf(_help_name, "", Array[OptionSpec](), [
        ArgSpec.string("command" where default' = "")
      ])
      _commands.update(_help_name, help_cmd)
    end

  fun box name(): String => _name

  fun box descr(): String => _descr

  fun box options(): Map[String, OptionSpec] box => _options

  fun box commands(): Map[String, CommandSpec box] box => _commands

  fun box args(): Array[ArgSpec] box => _args

  fun box help_name(): String => _help_name

  fun box help_string(): String =>
    let s = _name.clone()
    s.append(" ")
    for a in _args.values() do
      s.append(a.help_string())
    end
    s

class val OptionSpec
  """
  OptionSpec describes the specification of a named Option. They have a name,
  descr(iption), a short-name, a typ(e), and a default value when they are not
  required.

  Options can be placed anywhere before or after commands, and can be thought of
  as named arguments.
  """
  let _name: String
  let _descr: String
  let _short: (U8 | None)
  let _typ: ValueType
  let _default: Value
  let _required: Bool

  fun tag _init(typ': ValueType, default': (Value | None))
    : (ValueType, Value, Bool)
  =>
    match default'
    | None => (typ', false, true)
    | let d: Value => (typ', d, false)
    else
      // Ponyc limitation: else can't happen, but segfaults without it
      (BoolType, false, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (Bool | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (String | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(StringType, default')

  new val i64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (I64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(I64Type, default')

  new val f64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (F64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(F64Type, default')

  fun name(): String => _name

  fun descr(): String => _descr

  fun typ(): ValueType => _typ

  fun default(): Value => _default

  fun required(): Bool => _required

  // Other than bools, all options require args.
  fun _requires_arg(): Bool =>
    match _typ
    | let _: BoolType => false
    else
      true
    end

  // Used for bool options to get the true arg when option is present w/o arg
  fun _default_arg(): Value =>
    match _typ
    | let _: BoolType => true
    else
      false
    end

  fun _has_short(sh: U8): Bool =>
    match _short
    | let ss: U8 => sh == ss
    else
      false
    end

  fun help_string(): String =>
    let s =
      match _short
      | let ss: U8 => "-" + String.from_utf32(ss.u32()) + ", "
      else
        "    "
      end
    s + "--" + _name +
      if not _required then "=" + _default.string() else "" end

  fun deb_string(): String =>
    "--" + _name + "[" + _typ.string() + "]" +
      if not _required then "(=" + _default.string() + ")" else "" end

class val ArgSpec
  """
  ArgSpec describes the specification of a positional Arg(ument). They have a
  name, descr(iption), a typ(e), and a default value when they are not required.

  Args always come after a leaf command, and are assigned in their positional
  order.
  """
  let _name: String
  let _descr: String
  let _typ: ValueType
  let _default: Value
  let _required: Bool

  fun tag _init(typ': ValueType, default': (Value | None))
    : (ValueType, Value, Bool)
  =>
    match default'
    | None => (typ', false, true)
    | let d: Value => (typ', d, false)
    else
      // Ponyc limitation: else can't happen, but segfaults without it
      (BoolType, false, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    default': (Bool | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    default': (String | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(StringType, default')

  new val i64(name': String,
    descr': String = "",
    default': (I64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(I64Type, default')

  new val f64(name': String,
    descr': String = "",
    default': (F64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(F64Type, default')

  fun name(): String => _name

  fun descr(): String => _descr

  fun typ(): ValueType => _typ

  fun default(): Value => _default

  fun required(): Bool => _required

  fun help_string(): String =>
    "<" + _name + ">"

  fun deb_string(): String =>
    _name + "[" + _typ.string() + "]" +
      if not _required then "(=" + _default.string() + ")" else "" end

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
