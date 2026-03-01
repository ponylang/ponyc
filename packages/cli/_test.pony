use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestBadName)
    test(_TestBools)
    test(_TestChat)
    test(_TestDefaults)
    test(_TestDefaultWithSub)
    test(_TestDuplicate)
    test(_TestEnvs)
    test(_TestHelp)
    test(_TestHelpFalse)
    test(_TestHelpMultipleArgs)
    test(_TestHyphenArg)
    test(_TestLongsEq)
    test(_TestLongsNext)
    test(_TestMinimal)
    test(_TestMinimalWithHelp)
    test(_TestMultipleEndOfOptions)
    test(_TestMustBeLeaf)
    test(_TestOptionStop)
    test(_TestShortsAdj)
    test(_TestShortsEq)
    test(_TestShortsNext)
    test(_TestUnexpectedArg)
    test(_TestUnknownCommand)
    test(_TestUnknownLong)
    test(_TestUnknownShort)

class \nodoc\ iso _TestMinimal is UnitTest
  fun name(): String => "cli/minimal"

  fun apply(h: TestHelper) ? =>
    let cs = CommandSpec.leaf("minimal", "", [
      OptionSpec.bool("aflag", "")
    ])?

    h.assert_eq[String]("minimal", cs.name())
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = ["ignored"; "--aflag=true"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("minimal", cmd.fullname())
    h.assert_eq[Bool](true, cmd.option("aflag").bool())

class \nodoc\ iso _TestMinimalWithHelp is UnitTest
  fun name(): String => "cli/minimal_help"

  fun apply(h: TestHelper) ? =>
    let cs = CommandSpec.leaf("minimal_help", "",[])? .> add_help()?
    h.assert_eq[String]("minimal_help", cs.name())
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = ["--help"]
    // test for successful parsing
    let cmdErr = CommandParser(cs).parse(args) as Command

class \nodoc\ iso _TestBadName is UnitTest
  fun name(): String => "cli/badname"

  // Negative test: command names must be alphanum tokens
  fun apply(h: TestHelper) =>
    try
      let cs = CommandSpec.leaf("min imal", "")?
      h.fail("expected error on bad command name: " + cs.name())
    end

class \nodoc\ iso _TestUnknownCommand is UnitTest
  fun name(): String => "cli/unknown_command"

  // Negative test: unknown command should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?
    h.assert_false(cs.is_leaf())
    h.assert_true(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String]("Error: unknown command at: 'unknown'", se.string())
    else
      h.fail("expected syntax error for unknown command: " + cmdErr.string())
    end

class \nodoc\ iso _TestUnexpectedArg is UnitTest
  fun name(): String => "cli/unexpected_arg"

  // Negative test: unexpected arg/command token should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String](
        "Error: too many positional arguments at: 'unknown'", se.string())
    else
      h.fail("expected syntax error for unknown command: " + cmdErr.string())
    end

class \nodoc\ iso _TestUnknownShort is UnitTest
  fun name(): String => "cli/unknown_short"

  // Negative test: unknown short option should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "-Z"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String]("Error: unknown short option at: 'Z'", se.string())
    else
      h.fail(
        "expected syntax error for unknown short option: " + cmdErr.string())
    end

class \nodoc\ iso _TestUnknownLong is UnitTest
  fun name(): String => "cli/unknown_long"

  // Negative test: unknown long option should report
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "--unknown"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
      h.assert_eq[String](
        "Error: unknown long option at: 'unknown'", se.string())
    else
      h.fail(
        "expected syntax error for unknown long option: " + cmdErr.string())
    end

class \nodoc\ iso _TestHyphenArg is UnitTest
  fun name(): String => "cli/hyphen"

  // Rule 1
  fun apply(h: TestHelper) ? =>
    let cs = CommandSpec.leaf("minimal" where args' = [
      ArgSpec.string("name", "")
    ])?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())
    let args: Array[String] = ["ignored"; "-"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("minimal", cmd.fullname())
    h.assert_eq[String]("-", cmd.arg("name").string())

class \nodoc\ iso _TestBools is UnitTest
  fun name(): String => "cli/bools"

  // Rules 2, 3, 5, 7 w/ Bools
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.bools_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = ["ignored"; "-ab"; "-c=true"; "-d=false"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("bools", cmd.fullname())
    h.assert_eq[Bool](true, cmd.option("aaa").bool())
    h.assert_eq[Bool](true, cmd.option("bbb").bool())
    h.assert_eq[Bool](true, cmd.option("ccc").bool())
    h.assert_eq[Bool](false, cmd.option("ddd").bool())

class \nodoc\ iso _TestDefaults is UnitTest
  fun name(): String => "cli/defaults"

  // Rules 2, 3, 5, 6
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] =
      ["ignored"; "-B"; "-S--"; "-I42"; "-U47"; "-F42.0"]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolo").bool())
    h.assert_eq[String]("astring", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("into").i64())
    h.assert_eq[U64](47, cmd.option("uinto").u64())
    h.assert_eq[F64](42.0, cmd.option("floato").f64())
    h.assert_eq[USize](0, cmd.option("stringso").string_seq().size())

class \nodoc\ iso _TestDefaultWithSub is UnitTest
  fun name(): String => "cli/default_with_sub"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.default_with_sub_spec()?
    h.assert_true(cs.is_parent())

    let cmd = CommandParser(cs).parse([ "cmd"; "sub" ]) as Command

    h.assert_eq[String]("foo", cmd.option("arg").string())

class \nodoc\ iso _TestShortsAdj is UnitTest
  fun name(): String => "cli/shorts_adjacent"

  // Rules 2, 3, 5, 6, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?
    h.assert_true(cs.is_leaf())
    h.assert_false(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "-BS--"; "-I42"; "-U47"; "-F42.0"; "-zaaa"; "-zbbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0)?)
    h.assert_eq[String]("bbb", stringso.string_seq()(1)?)

    let ambiguous_args: Array[String] = [
      "ignored"; "-swords=with=signs"
    ]
    let cmdSyntax = CommandParser(cs).parse(ambiguous_args)
    match \exhaustive\ cmdSyntax
    | let se: SyntaxError =>
      h.assert_eq[String](
        "Error: ambiguous args for short option at: 's'", se.string())
    else
      h.fail("expected syntax error for ambiguous args: " + cmdSyntax.string())
    end

class \nodoc\ iso _TestShortsEq is UnitTest
  fun name(): String => "cli/shorts_eq"

  // Rules 2, 3, 5, 7
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS=astring"; "-s=words=with=signs"
      "-I=42"; "-U=47"; "-F=42.0"; "-z=aaa"; "-z=bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[String]("words=with=signs", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0)?)
    h.assert_eq[String]("bbb", stringso.string_seq()(1)?)

class \nodoc\ iso _TestShortsNext is UnitTest
  fun name(): String => "cli/shorts_next"

  // Rules 2, 3, 5, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS"; "--"; "-s"; "words=with=signs"
      "-I"; "42"; "-U"; "47"
      "-F"; "42.0"; "-z"; "aaa"; "-z"; "bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[String]("words=with=signs", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0)?)
    h.assert_eq[String]("bbb", stringso.string_seq()(1)?)

class \nodoc\ iso _TestLongsEq is UnitTest
  fun name(): String => "cli/longs_eq"

  // Rules 4, 5, 7
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr=true"; "--stringr=astring"; "--stringo=words=with=signs"
      "--intr=42"; "--uintr=47"; "--floatr=42.0"
      "--stringso=aaa"; "--stringso=bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[String]("words=with=signs", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0)?)
    h.assert_eq[String]("bbb", stringso.string_seq()(1)?)

class \nodoc\ iso _TestLongsNext is UnitTest
  fun name(): String => "cli/longs_next"

  // Rules 4, 5, 8
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr"; "--stringr"; "--"; "--stringo"; "words=with=signs"
      "--intr"; "42"; "--uintr"; "47"; "--floatr"; "42.0"
      "--stringso"; "aaa"; "--stringso"; "bbb"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("--", cmd.option("stringr").string())
    h.assert_eq[String]("words=with=signs", cmd.option("stringo").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())
    let stringso = cmd.option("stringso")
    h.assert_eq[USize](2, stringso.string_seq().size())
    h.assert_eq[String]("aaa", stringso.string_seq()(0)?)
    h.assert_eq[String]("bbb", stringso.string_seq()(1)?)

class \nodoc\ iso _TestEnvs is UnitTest
  fun name(): String => "cli/envs"

  // Rules
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
    ]
    let envs: Array[String] = [
      "SIMPLE_BOOLR=true"
      "SIMPLE_STRINGR=astring"
      "SIMPLE_INTR=42"
      "SIMPLE_UINTR=47"
      "SIMPLE_FLOATR=42.0"
    ]
    let cmdErr = CommandParser(cs).parse(args, envs)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[Bool](true, cmd.option("boolr").bool())
    h.assert_eq[String]("astring", cmd.option("stringr").string())
    h.assert_eq[I64](42, cmd.option("intr").i64())
    h.assert_eq[U64](47, cmd.option("uintr").u64())
    h.assert_eq[F64](42.0, cmd.option("floatr").f64())

class \nodoc\ iso _TestOptionStop is UnitTest
  fun name(): String => "cli/option_stop"

  // Rules 2, 3, 5, 7, 9
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "-BS=astring"; "-I=42"; "-F=42.0"; "-U=23"
      "--"; "-f=1.0"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("-f=1.0", cmd.arg("words").string())
    h.assert_eq[F64](42.0, cmd.option("floato").f64())

class \nodoc\ iso _TestDuplicate is UnitTest
  fun name(): String => "cli/duplicate"

  // Rules 4, 5, 7, 10
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let args: Array[String] = [
      "ignored"
      "--boolr=true"; "--stringr=astring"; "--intr=42"; "--uintr=47"
      "--floatr=42.0"; "--stringr=newstring"
    ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("newstring", cmd.option("stringr").string())

class \nodoc\ iso _TestChat is UnitTest
  fun name(): String => "cli/chat"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?
    h.assert_false(cs.is_leaf())
    h.assert_true(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "--admin"; "--name=carl"; "say"; "-v80"; "hi"; "yo"; "hey"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command

    h.assert_eq[String]("chat/say", cmd.fullname())

    h.assert_eq[String]("say", cmd.spec().name())

    let f1 = cmd.option("admin")
    h.assert_eq[String]("admin", f1.spec().name())
    h.assert_eq[Bool](true, f1.bool())

    let f2 = cmd.option("name")
    h.assert_eq[String]("name", f2.spec().name())
    h.assert_eq[String]("carl", f2.string())

    let f3 = cmd.option("volume")
    h.assert_eq[String]("volume", f3.spec().name())
    h.assert_eq[F64](80.0, f3.f64())

    let a1 = cmd.arg("words")
    h.assert_eq[String]("words", a1.spec().name())
    let words = a1.string_seq()
    h.assert_eq[USize](3, words.size())
    h.assert_eq[String]("hi", words(0)?)
    h.assert_eq[String]("yo", words(1)?)
    h.assert_eq[String]("hey", words(2)?)

class \nodoc\ iso _TestMustBeLeaf is UnitTest
  fun name(): String => "cli/must_be_leaf"

  // Negative test: can't just supply parent command
  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?
    h.assert_false(cs.is_leaf())
    h.assert_true(cs.is_parent())

    let args: Array[String] = [
      "ignored"
      "--admin"; "--name=carl"; "config"
    ]

    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    match cmdErr
    | let se: SyntaxError => None
    else
      h.fail("expected syntax error for non-leaf command: " + cmdErr.string())
    end

class \nodoc\ iso _TestHelp is UnitTest
  fun name(): String => "cli/help"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.chat_cli_spec()?

    let chErr = Help.for_command(cs, ["config"; "server"])
    let ch = chErr as CommandHelp

    let help = ch.help_string()
    h.log(help)
    h.assert_true(help.contains("Address of the server"))

class \nodoc\ iso _TestHelpFalse is UnitTest
  fun name(): String => "cli/help-false"

  fun apply(h: TestHelper) ? =>
    let cs =
      CommandSpec.leaf("bools", "A sample CLI with four bool options", [
        OptionSpec.string("name" where short' = 'n', default' = "John")
      ])?.>add_help()?
    let args = [
       "ignored"
       "--help=false"
    ]
    let cmd = CommandParser(cs).parse(args)
    match \exhaustive\ cmd
    | let c: Command =>
      h.assert_false(c.option("help").bool())
      h.assert_eq[String]("John", c.option("name").string())
    | let ch: CommandHelp => h.fail("--help=false is interpretet as demanding help output.")
    | let se: SyntaxError =>
      h.fail("--help=false is not handled correctly: " + se.string())
    end

class \nodoc\ iso _TestHelpMultipleArgs is UnitTest
  fun name(): String => "cli/help-multiple-args"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.simple_cli_spec()?

    let help = cs.help_string()
    h.log(help)
    h.assert_true(
      help.contains("simple <words> <argz>"))

class \nodoc\ iso _TestMultipleEndOfOptions is UnitTest
  fun name(): String => "cli/multiple-end-of-options"

  fun apply(h: TestHelper) ? =>
    let cs = _Fixtures.corral_spec()?

    let args: Array[String] = ["ignored"; "exec"; "--"; "lldb"; "ponyc"; "--"; "-v" ]
    let cmdErr = CommandParser(cs).parse(args)
    h.log("Parsed: " + cmdErr.string())

    let cmd = cmdErr as Command
    h.assert_eq[String]("corral/exec", cmd.fullname())

    let argss = cmd.arg("args").string_seq()
    h.assert_eq[String]("lldb", argss(0)?)
    h.assert_eq[String]("ponyc", argss(1)?)
    h.assert_eq[String]("--", argss(2)?)
    h.assert_eq[String]("-v", argss(3)?)

primitive _Fixtures
  fun bools_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a CLI with four bool options.
    """
    CommandSpec.leaf("bools", "A sample CLI with four bool options", [
      OptionSpec.bool("aaa" where short' = 'a')
      OptionSpec.bool("bbb" where short' = 'b')
      OptionSpec.bool("ccc" where short' = 'c')
      OptionSpec.bool("ddd" where short' = 'd')
    ])?

  fun simple_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a CLI with short options of each type.
    """
    CommandSpec.leaf("simple",
        "A sample program with various short options, optional and required", [
      OptionSpec.bool("boolr" where short' = 'B')
      OptionSpec.bool("boolo" where short' = 'b', default' = true)
      OptionSpec.string("stringr" where short' = 'S')
      OptionSpec.string("stringo" where short' = 's', default' = "astring")
      OptionSpec.i64("intr" where short' = 'I')
      OptionSpec.i64("into" where short' = 'i', default' = I64(42))
      OptionSpec.u64("uintr" where short' = 'U')
      OptionSpec.u64("uinto" where short' = 'u', default' = U64(47))
      OptionSpec.f64("floatr" where short' = 'F')
      OptionSpec.f64("floato" where short' = 'f', default' = F64(42.0))
      OptionSpec.string_seq("stringso" where short' = 'z')
    ], [
      ArgSpec.string("words" where default' = "hello")
      ArgSpec.string_seq("argz")
    ])?

  fun chat_cli_spec(): CommandSpec box ? =>
    """
    Builds and returns the spec for a sample chat client's CLI.
    """
    CommandSpec.parent("chat", "A sample chat program", [
      OptionSpec.bool("admin", "Chat as admin" where default' = false)
      OptionSpec.string("name", "Your name" where short' = 'n')
      OptionSpec.f64("volume", "Chat volume" where short' = 'v', default' = 1.0)
    ], [
      CommandSpec.leaf("say", "Say something", Array[OptionSpec](), [
        ArgSpec.string_seq("words", "The words to say")
      ])?
      CommandSpec.leaf("emote", "Send an emotion", [
        OptionSpec.f64("speed", "Emote play speed" where default' = F64(1.0))
      ], [
        ArgSpec.string("emotion", "Emote to send")
      ])?
      CommandSpec.parent("config", "Configuration commands",
        Array[OptionSpec](), [
        CommandSpec.leaf("server", "Server configuration", Array[OptionSpec](), [
          ArgSpec.string("address", "Address of the server")
        ])?
      ])?
    ])?


  fun corral_spec(): CommandSpec box ? =>
    """
    A snippet from Corral's command spec to demonstrate multiple end of option arguments
    """
    CommandSpec.parent("corral", "", [
        OptionSpec.u64(
          "debug",
          "Configure debug output: 0=off, 1=err, 2=warn, 3=info, 4=fine."
          where short'='g',
          default' = 0)
      ], [
      CommandSpec.leaf(
        "exec",
        "For executing shell commands which require user interaction",
        Array[OptionSpec](),
        [
          ArgSpec.string_seq("args", "Arguments to run.")
        ])?
    ])?

  fun default_with_sub_spec(): CommandSpec box ? =>
    let root = CommandSpec.parent(
      "cmd",
      "Main command",
      [ OptionSpec.string("arg", "an arg" where default' = "foo") ])?
    let sub = CommandSpec.leaf("sub", "Sub command")?

    root.add_command(sub)?
    root.add_help()?
    root
