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

  fun name(): String => _name

  fun descr(): String => _descr

  fun options(): Map[String, OptionSpec] box => _options

  fun commands(): Map[String, CommandSpec box] box => _commands

  fun args(): Array[ArgSpec] box => _args

  fun help_name(): String => _help_name

  fun help_string(): String =>
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
  let _typ: _ValueType
  let _default: _Value
  let _required: Bool

  fun tag _init(typ': _ValueType, default': (_Value | None))
    : (_ValueType, _Value, Bool)
  =>
    match default'
    | None => (typ', false, true)
    | let d: _Value => (typ', d, false)
    else
      // Ponyc limitation: else can't happen, but segfaults without it
      (_BoolType, false, false)
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
    (_typ, _default, _required) = _init(_BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (String | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_StringType, default')

  new val i64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (I64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_I64Type, default')

  new val f64(name': String,
    descr': String = "",
    short': (U8 | None) = None,
    default': (F64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_F64Type, default')

  new val string_seq(
    name': String,
    descr': String = "",
    short': (U8 | None) = None)
  =>
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_StringSeqType, _StringSeq.empty())

  fun name(): String => _name

  fun descr(): String => _descr

  fun _typ_p(): _ValueType => _typ

  fun _default_p(): _Value => _default

  fun required(): Bool => _required

  // Other than bools, all options require args.
  fun _requires_arg(): Bool =>
    match _typ
    | let _: _BoolType => false
    else
      true
    end

  // Used for bool options to get the true arg when option is present w/o arg
  fun _default_arg(): _Value =>
    match _typ
    | let _: _BoolType => true
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
  let _typ: _ValueType
  let _default: _Value
  let _required: Bool

  fun tag _init(typ': _ValueType, default': (_Value | None))
    : (_ValueType, _Value, Bool)
  =>
    match default'
    | None => (typ', false, true)
    | let d: _Value => (typ', d, false)
    else
      // Ponyc limitation: else can't happen, but segfaults without it
      (_BoolType, false, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    default': (Bool | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    default': (String | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_StringType, default')

  new val i64(name': String,
    descr': String = "",
    default': (I64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_I64Type, default')

  new val f64(name': String,
    descr': String = "",
    default': (F64 | None) = None)
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_F64Type, default')

  new val string_seq(
    name': String,
    descr': String = "")
  =>
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_StringSeqType, _StringSeq.empty())

  fun name(): String => _name

  fun descr(): String => _descr

  fun _typ_p(): _ValueType => _typ

  fun _default_p(): _Value => _default

  fun required(): Bool => _required

  fun help_string(): String =>
    "<" + _name + ">"

  fun deb_string(): String =>
    _name + "[" + _typ.string() + "]" +
      if not _required then "(=" + _default.string() + ")" else "" end

class _StringSeq is ReadSeq[String]
  let strings: Array[String]

  new val empty() =>
    strings = Array[String]

  new val from(s: String) =>
    strings = Array[String].init(s, 1)

  new val combined(ss0: _StringSeq val, ss1: _StringSeq val) =>
    let ss = ss0.strings.clone()
    ss.append(ss1.strings)
    strings = ss

  fun string(): String iso^ =>
    let str = recover String() end
    str.push('[')
    for s in strings.values() do
      if str.size() > 1 then str.push(',') end
      str.append(s)
    end
    str.push(']')
    str

  fun size(): USize => strings.size()
  fun apply(i: USize): this->String ? => strings(i)
  fun values(): Iterator[this->String]^ => strings.values()

type _Value is (Bool | String | I64 | F64 | _StringSeq val)

trait val _ValueType
  fun string(): String
  fun value_of(s: String): _Value ?
  fun is_seq(): Bool => false
  fun append(v1: _Value, v2: _Value): _Value => v1

primitive _BoolType is _ValueType
  fun string(): String => "Bool"
  fun value_of(s: String): _Value ? => s.bool()

primitive _StringType is _ValueType
  fun string(): String => "String"
  fun value_of(s: String): _Value => s

primitive _I64Type is _ValueType
  fun string(): String => "I64"
  fun value_of(s: String): _Value ? => s.i64()

primitive _F64Type is _ValueType
  fun string(): String => "F64"
  fun value_of(s: String): _Value => s.f64()

primitive _StringSeqType is _ValueType
  fun string(): String => "ReadSeq[String]"
  fun value_of(s: String): _Value => _StringSeq.from(s)
  fun is_seq(): Bool => true
  fun append(v1: _Value, v2: _Value): _Value =>
    try
      let co = _StringSeq.combined(v1 as _StringSeq val, v2 as _StringSeq val)
      co
    else
      v1
    end
