use "collections"
use pc = "collections/persistent"

primitive _CommandSpecLeaf
primitive _CommandSpecParent

type _CommandSpecType is (_CommandSpecLeaf | _CommandSpecParent )

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
  let _type: _CommandSpecType
  let _name: String
  let _descr: String
  let _options: Map[String, OptionSpec] = _options.create()
  var _help_name: String = ""
  var _help_info: String = ""

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
    Creates a command spec that can accept options and child commands, but not
    arguments.
    """
    _type = _CommandSpecParent
    _name = _assertName(name')?
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
    Creates a command spec that can accept options and arguments, but not child
    commands.
    """
    _type = _CommandSpecLeaf
    _name = _assertName(name')?
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
    Adds an additional child command to this parent command.
    """
    if is_leaf() then error end
    _commands.update(cmd.name(), cmd)

  fun ref add_help(hname: String = "help", descr': String = "") ? =>
    """
    Adds a standard help option and, optionally command, to a root command.
    """
    _help_name = hname
    _help_info = descr'
    let help_option = OptionSpec.bool(_help_name, _help_info, 'h', false)
    _options.update(_help_name, help_option)
    if is_parent() then
      let help_cmd = CommandSpec.leaf(_help_name, "", Array[OptionSpec](), [
        ArgSpec.string("command" where default' = "")
      ])?
      _commands.update(_help_name, help_cmd)
    end

  fun name(): String =>
    """
    Returns the name of this command.
    """
    _name

  fun descr(): String =>
    """
    Returns the description for this command.
    """
    _descr

  fun options(): Map[String, OptionSpec] box =>
    """
    Returns a map by name of the named options of this command.
    """
    _options

  fun commands(): Map[String, CommandSpec box] box =>
    """
    Returns a map by name of the child commands of this command.
    """
    _commands

  fun args(): Array[ArgSpec] box =>
    """
    Returns an array of the positional arguments of this command.
    """
    _args

  fun is_leaf(): Bool => _type is _CommandSpecLeaf

  fun is_parent(): Bool => _type is _CommandSpecParent

  fun help_name(): String =>
    """
    Returns the name of the help command, which defaults to "help".
    """
    _help_name

  fun help_string(): String =>
    """
    Returns a formated help string for this command and all of its arguments.
    """
    let s = _name.clone()
    s.append(" ")
    let args_iter = _args.values()
    for a in args_iter do
      s.append(a.help_string())
      if args_iter.has_next() then s.append(" ") end
    end
    s

class val OptionSpec
  """
  OptionSpec describes the specification of a named Option. They have a name,
  descr(iption), a short-name, a typ(e), and a default value when they are not
  required.

  Options can be placed anywhere before or after commands, and can be thought
  of as named arguments.
  """
  let _name: String
  let _descr: String
  let _short: (U32 | None)
  let _typ: _ValueType
  let _default: _Value
  let _required: Bool

  fun tag _init(typ': _ValueType, default': (_Value | None))
    : (_ValueType, _Value, Bool)
  =>
    match default'
    | None => (typ', false, true)
    | let d: _Value => (typ', d, false)
    end

  new val bool(
    name': String,
    descr': String = "",
    short': (U32 | None) = None,
    default': (Bool | None) = None)
  =>
    """
    Creates an Option with a Bool typed value that can be used like
      `--opt` or `-O` or `--opt=true` or `-O=true`
    to yield an option value like
      `cmd.option("opt").bool() == true`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    short': (U32 | None) = None,
    default': (String | None) = None)
  =>
    """
    Creates an Option with a String typed value that can be used like
      `--file=dir/filename` or `-F=dir/filename` or `-Fdir/filename`
    to yield an option value like
      `cmd.option("file").string() == "dir/filename"`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_StringType, default')

  new val i64(name': String,
    descr': String = "",
    short': (U32 | None) = None,
    default': (I64 | None) = None)
  =>
    """
    Creates an Option with an I64 typed value that can be used like
      `--count=42 -C=42`
    to yield an option value like
      `cmd.option("count").i64() == I64(42)`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_I64Type, default')

  new val u64(name': String,
    descr': String = "",
    short': (U32 | None) = None,
    default': (U64 | None) = None)
  =>
    """
    Creates an Option with an U64 typed value that can be used like
      `--count=47 -C=47`
    to yield an option value like
      `cmd.option("count").u64() == U64(47)`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_U64Type, default')

  new val f64(name': String,
    descr': String = "",
    short': (U32 | None) = None,
    default': (F64 | None) = None)
  =>
    """
    Creates an Option with a F64 typed value that can be used like
      `--ratio=1.039` or `-R=1.039`
    to yield an option value like
      `cmd.option("ratio").f64() == F64(1.039)`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_F64Type, default')

  new val string_seq(
    name': String,
    descr': String = "",
    short': (U32 | None) = None)
  =>
    """
    Creates an Option with a ReadSeq[String] typed value that can be used like
      `--files=file1 --files=files2 --files=files2`
    to yield a sequence of three strings equivalent to
      `cmd.option("ratio").string_seq() (equiv) ["file1"; "file2"; "file3"]`.
    """
    _name = name'
    _descr = descr'
    _short = short'
    (_typ, _default, _required) = _init(_StringSeqType, _StringSeq.empty())

  fun name(): String =>
    """
    Returns the name of this option.
    """
    _name

  fun descr(): String =>
    """
    Returns the description for this option.
    """
    _descr

  fun _typ_p(): _ValueType => _typ

  fun _default_p(): _Value => _default

  fun required(): Bool =>
    """
    Returns true iff this option is required to be present in the command line.
    """
    _required

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

  fun _has_short(sh: U32): Bool =>
    match _short
    | let ss: U32 => sh == ss
    else
      false
    end

  fun help_string(): String =>
    """
    Returns a formated help string for this option.
    """
    let s =
      match _short
      | let ss: U32 => "-" + String.from_utf32(ss) + ", "
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
  name, descr(iption), a typ(e), and a default value when they are not
  required.

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
    end

  new val bool(
    name': String,
    descr': String = "",
    default': (Bool | None) = None)
  =>
    """
    Creates an Arg with a Bool typed value that can be used like
      `<cmd> true`
    to yield an arg value like
      `cmd.arg("opt").bool() == true`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_BoolType, default')

  new val string(
    name': String,
    descr': String = "",
    default': (String | None) = None)
  =>
    """
    Creates an Arg with a String typed value that can be used like
      `<cmd> filename`
    to yield an arg value
      `cmd.arg("file").string() == "filename"`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_StringType, default')

  new val i64(name': String,
    descr': String = "",
    default': (I64 | None) = None)
  =>
    """
    Creates an Arg with an I64 typed value that can be used like
      `<cmd> 42`
    to yield an arg value like
      `cmd.arg("count").i64() == I64(42)`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_I64Type, default')

  new val u64(name': String,
    descr': String = "",
    default': (U64 | None) = None)
  =>
    """
    Creates an Arg with an U64 typed value that can be used like
      `<cmd> 47`
    to yield an arg value like
      `cmd.arg("count").u64() == U64(47)`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_U64Type, default')

  new val f64(name': String,
    descr': String = "",
    default': (F64 | None) = None)
  =>
    """
    Creates an Arg with a F64 typed value that can be used like
      `<cmd> 1.039`
    to yield an arg value like
      `cmd.arg("ratio").f64() == F64(1.039)`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_F64Type, default')

  new val string_seq(
    name': String,
    descr': String = "")
  =>
    """
    Creates an Arg with a ReadSeq[String] typed value that can be used like
      `<cmd> file1 file2 file3`
    to yield a sequence of three strings equivalent to
      `cmd.arg("file").string_seq() (equiv) ["file1"; "file2"; "file3"]`.
    """
    _name = name'
    _descr = descr'
    (_typ, _default, _required) = _init(_StringSeqType, _StringSeq.empty())

  fun name(): String =>
    """
    Returns the name of this arg.
    """
    _name

  fun descr(): String =>
    """
    Returns the description for this arg.
    """
    _descr

  fun _typ_p(): _ValueType => _typ

  fun _default_p(): _Value => _default

  fun required(): Bool =>
    """
    Returns true iff this arg is required to be present in the command line.
    """
    _required

  fun help_string(): String =>
    """
    Returns a formated help string for this arg.
    """
    "<" + _name + ">"

  fun deb_string(): String =>
    _name + "[" + _typ.string() + "]" +
      if not _required then "(=" + _default.string() + ")" else "" end

class _StringSeq is ReadSeq[String]
  """
  _StringSeq is a wrapper / helper class for working with String sequence
  values while parsing. It assists in collecting the strings as they are
  parsed, and producing a ReadSeq[String] as a result.
  """
  let strings: pc.Vec[String]

  new val empty() =>
    strings = pc.Vec[String]

  new val from_string(s: String) =>
    strings = (pc.Vec[String]).push(s)

  new val from_concat(ss0: _StringSeq val, ss1: _StringSeq val) =>
    strings = ss0.strings.concat(ss1.strings.values())

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
  fun apply(i: USize): this->String ? => strings(i)?
  fun values(): Iterator[this->String]^ => strings.values()

type _Value is (Bool | String | I64 | U64 | F64 | _StringSeq val)

trait val _ValueType
  fun string(): String
  fun value_of(s: String): _Value ?
  fun is_seq(): Bool => false
  fun append(v1: _Value, v2: _Value): _Value => v1

primitive _BoolType is _ValueType
  fun string(): String => "Bool"
  fun value_of(s: String): _Value ? => s.bool()?

primitive _StringType is _ValueType
  fun string(): String => "String"
  fun value_of(s: String): _Value => s

primitive _I64Type is _ValueType
  fun string(): String => "I64"
  fun value_of(s: String): _Value ? => s.i64()?

primitive _U64Type is _ValueType
  fun string(): String => "U64"
  fun value_of(s: String): _Value ? => s.u64()?

primitive _F64Type is _ValueType
  fun string(): String => "F64"
  fun value_of(s: String): _Value ? => s.f64()?

primitive _StringSeqType is _ValueType
  fun string(): String => "ReadSeq[String]"
  fun value_of(s: String): _Value => _StringSeq.from_string(s)
  fun is_seq(): Bool => true
  fun append(v1: _Value, v2: _Value): _Value =>
    """
    When is_seq() returns true, append() is called during parsing to append
    a new parsed value onto an existing value.
    """
    try
      _StringSeq.from_concat(v1 as _StringSeq val, v2 as _StringSeq val)
    else
      v1
    end
